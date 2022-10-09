/*
    protocol_auth.c -- handle the meta-protocol, authentication
    Copyright (C) 2014-2017 Guus Sliepen <guus@meshlink.io>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "system.h"

#include "conf.h"
#include "connection.h"
#include "devtools.h"
#include "ecdsa.h"
#include "edge.h"
#include "graph.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "meta.h"
#include "net.h"
#include "netutl.h"
#include "node.h"
#include "packmsg.h"
#include "prf.h"
#include "protocol.h"
#include "sptps.h"
#include "utils.h"
#include "xalloc.h"
#include "ed25519/sha512.h"

#include <assert.h>

extern bool node_write_devclass(meshlink_handle_t *mesh, node_t *n);

bool send_id(meshlink_handle_t *mesh, connection_t *c) {
	return send_request(mesh, c, NULL, "%d %s %d.%d %s %u", ID, mesh->self->name, PROT_MAJOR, PROT_MINOR, mesh->appname, 0);
}

static bool commit_invitation(meshlink_handle_t *mesh, connection_t *c, const void *data) {
	// Check if the node is known
	node_t *n = lookup_node(mesh, c->name);

	if(n) {
		if(n->status.blacklisted) {
			logger(mesh, MESHLINK_ERROR, "Invitee %s is blacklisted", c->name);
		} else {
			logger(mesh, MESHLINK_ERROR, "Invitee %s already known", c->name);
		}

		return false;
	}

	// Create a new node
	n = new_node();
	n->name = xstrdup(c->name);
	n->devclass = DEV_CLASS_UNKNOWN;
	n->ecdsa = ecdsa_set_public_key(data);
	n->submesh = c->submesh;

	// Remember its current address
	node_add_recent_address(mesh, n, &c->address);

	if(!node_write_config(mesh, n, true)) {
		logger(mesh, MESHLINK_ERROR, "Error writing configuration file for invited node %s!\n", c->name);
		free_node(n);
		return false;

	}

	node_add(mesh, n);

	logger(mesh, MESHLINK_INFO, "Key successfully received from %s", c->name);

	//TODO: callback to application to inform of an accepted invitation

	sptps_send_record(&c->sptps, 1, "", 0);

	return true;
}

static bool process_invitation(meshlink_handle_t *mesh, connection_t *c, const void *data) {
	// Recover the filename from the cookie and the key
	char *fingerprint = ecdsa_get_base64_public_key(mesh->invitation_key);
	char hash[64];
	char hashbuf[18 + strlen(fingerprint)];
	char cookie[25];
	memcpy(hashbuf, data, 18);
	memcpy(hashbuf + 18, fingerprint, sizeof(hashbuf) - 18);
	sha512(hashbuf, sizeof(hashbuf), hash);
	b64encode_urlsafe(hash, cookie, 18);
	free(fingerprint);

	config_t config;

	if(!invitation_read(mesh, cookie, &config)) {
		logger(mesh, MESHLINK_ERROR, "Error while trying to read invitation file\n");
		return false;
	}

	// Read the new node's Name from the file
	packmsg_input_t in = {config.buf, config.len};
	uint32_t version = packmsg_get_uint32(&in);

	if(version != MESHLINK_INVITATION_VERSION) {
		logger(mesh, MESHLINK_ERROR, "Invalid invitation file\n");
		config_free(&config);
		return false;
	}

	int64_t timestamp = packmsg_get_int64(&in);

	if(time(NULL) >= timestamp + mesh->invitation_timeout) {
		logger(mesh, MESHLINK_ERROR, "Peer tried to use an outdated invitation file %s\n", cookie);
		config_free(&config);
		return false;
	}

	char *name = packmsg_get_str_dup(&in);

	if(!check_id(name)) {
		logger(mesh, MESHLINK_ERROR, "Invalid invitation file %s\n", cookie);
		free(name);
		config_free(&config);
		return false;
	}

	free(c->name);
	c->name = name;

	// Check if the file contains Sub-Mesh information
	char *submesh_name = packmsg_get_str_dup(&in);

	if(!strcmp(submesh_name, CORE_MESH)) {
		free(submesh_name);
		c->submesh = NULL;
	} else {
		if(!check_id(submesh_name)) {
			logger(mesh, MESHLINK_ERROR, "Invalid invitation file %s\n", cookie);
			free(submesh_name);
			config_free(&config);
			return false;
		}

		c->submesh = lookup_or_create_submesh(mesh, submesh_name);
		free(submesh_name);

		if(!c->submesh) {
			logger(mesh, MESHLINK_ERROR, "Unknown submesh in invitation file %s\n", cookie);
			config_free(&config);
			return false;
		}
	}

	if(mesh->inviter_commits_first && !commit_invitation(mesh, c, (const char *)data + 18)) {
		config_free(&config);
		return false;
	}

	if(mesh->inviter_commits_first) {
		devtool_set_inviter_commits_first(true);
	}

	// Send the node the contents of the invitation file
	sptps_send_record(&c->sptps, 0, config.buf, config.len);

	config_free(&config);

	c->status.invitation_used = true;

	logger(mesh, MESHLINK_INFO, "Invitation %s successfully sent to %s", cookie, c->name);
	return true;
}

static bool receive_invitation_sptps(void *handle, uint8_t type, const void *data, uint16_t len) {
	connection_t *c = handle;
	meshlink_handle_t *mesh = c->mesh;

	// Extend the time for the invitation exchange upon receiving a valid message
	c->last_ping_time = mesh->loop.now.tv_sec;

	if(type == SPTPS_HANDSHAKE) {
		// The peer should send its cookie first.
		return true;
	}

	if(mesh->inviter_commits_first) {
		if(type == 2 && len == 18 + 32 && !c->status.invitation_used) {
			return process_invitation(mesh, c, data);
		}
	} else {
		if(type == 0 && len == 18 && !c->status.invitation_used) {
			return process_invitation(mesh, c, data);
		} else if(type == 1 && len == 32 && c->status.invitation_used) {
			return commit_invitation(mesh, c, data);
		}
	}

	return false;
}

bool id_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	assert(request);
	assert(*request);

	char name[MAX_STRING_SIZE];

	if(sscanf(request, "%*d " MAX_STRING " %d.%d %*s %u", name, &c->protocol_major, &c->protocol_minor, &c->flags) < 2) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "ID", c->name);
		return false;
	}

	/* Check if this is an invitation  */

	if(name[0] == '?') {
		if(!mesh->invitation_key) {
			logger(mesh, MESHLINK_ERROR, "Got invitation from %s but we don't have an invitation key", c->name);
			return false;
		}

		c->ecdsa = ecdsa_set_base64_public_key(name + 1);

		if(!c->ecdsa) {
			logger(mesh, MESHLINK_ERROR, "Got bad invitation from %s", c->name);
			return false;
		}

		c->status.invitation = true;
		char *mykey = ecdsa_get_base64_public_key(mesh->invitation_key);

		if(!mykey) {
			return false;
		}

		if(!send_request(mesh, c, NULL, "%d %s", ACK, mykey)) {
			return false;
		}

		free(mykey);

		c->protocol_minor = 2;
		c->allow_request = 1;
		c->last_ping_time = mesh->loop.now.tv_sec;

		return sptps_start(&c->sptps, c, false, false, mesh->invitation_key, c->ecdsa, meshlink_invitation_label, sizeof(meshlink_invitation_label), send_meta_sptps, receive_invitation_sptps);
	}

	/* Check if identity is a valid name */

	if(!check_id(name)) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ID", c->name, "invalid name");
		return false;
	}

	/* If this is an outgoing connection, make sure we are connected to the right host */

	if(c->outgoing) {
		if(strcmp(c->name, name)) {
			logger(mesh, MESHLINK_ERROR, "Peer is %s instead of %s", name, c->name);
			return false;
		}
	} else {
		if(c->name) {
			free(c->name);
		}

		c->name = xstrdup(name);
	}

	/* Check if version matches */

	if(c->protocol_major != PROT_MAJOR) {
		logger(mesh, MESHLINK_ERROR, "Peer %s uses incompatible version %d.%d",
		       c->name, c->protocol_major, c->protocol_minor);
		return false;
	}

	/* Check if we know this node */

	node_t *n = lookup_node(mesh, c->name);

	if(!n) {
		logger(mesh, MESHLINK_ERROR, "Peer %s has unknown identity", c->name);
		return false;
	}

	if(!node_read_public_key(mesh, n)) {
		logger(mesh, MESHLINK_ERROR, "No key known for peer %s", c->name);

		if(n->status.reachable && !n->status.waitingforkey) {
			logger(mesh, MESHLINK_INFO, "Requesting key from peer %s", c->name);
			send_req_key(mesh, n);
		}

		return false;
	}

	/* Forbid version rollback for nodes whose ECDSA key we know */

	if(ecdsa_active(c->ecdsa) && c->protocol_minor < 2) {
		logger(mesh, MESHLINK_ERROR, "Peer %s tries to roll back protocol version to %d.%d",
		       c->name, c->protocol_major, c->protocol_minor);
		return false;
	}

	c->allow_request = ACK;
	c->last_ping_time = mesh->loop.now.tv_sec;
	char label[sizeof(meshlink_tcp_label) + strlen(mesh->self->name) + strlen(c->name) + 2];

	if(c->outgoing) {
		snprintf(label, sizeof(label), "%s %s %s", meshlink_tcp_label, mesh->self->name, c->name);
	} else {
		snprintf(label, sizeof(label), "%s %s %s", meshlink_tcp_label, c->name, mesh->self->name);
	}

	if(mesh->log_level <= MESHLINK_DEBUG) {
		char buf1[1024], buf2[1024];
		bin2hex((uint8_t *)mesh->private_key + 64, buf1, 32);
		bin2hex((uint8_t *)n->ecdsa + 64, buf2, 32);
		logger(mesh, MESHLINK_DEBUG, "Connection to %s mykey %s hiskey %s", c->name, buf1, buf2);
	}

	return sptps_start(&c->sptps, c, c->outgoing, false, mesh->private_key, n->ecdsa, label, sizeof(label) - 1, send_meta_sptps, receive_meta_sptps);
}

bool send_ack(meshlink_handle_t *mesh, connection_t *c) {
	node_t *n = lookup_node(mesh, c->name);

	if(n && n->status.blacklisted) {
		logger(mesh, MESHLINK_WARNING, "Peer %s is blacklisted", c->name);
		return send_error(mesh, c, BLACKLISTED, "blacklisted");
	}

	c->last_ping_time = mesh->loop.now.tv_sec;
	return send_request(mesh, c, NULL, "%d %s %d %x", ACK, mesh->myport, mesh->devclass, OPTION_PMTU_DISCOVERY | (PROT_MINOR << 24));
}

static void send_everything(meshlink_handle_t *mesh, connection_t *c) {
	/* Send all known subnets and edges */

	for splay_each(node_t, n, mesh->nodes) {
		for inner_splay_each(edge_t, e, n->edge_tree) {
			send_add_edge(mesh, c, e, 0);
		}
	}
}

bool ack_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	assert(request);
	assert(*request);

	char hisport[MAX_STRING_SIZE];
	int devclass;
	uint32_t options;
	node_t *n;

	if(sscanf(request, "%*d " MAX_STRING " %d %x", hisport, &devclass, &options) != 3) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "ACK", c->name);
		return false;
	}

	if(devclass < 0 || devclass >= DEV_CLASS_COUNT) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ACK", c->name, "devclass invalid");
		return false;
	}

	/* Check if we already have a node_t for him */

	n = lookup_node(mesh, c->name);

	if(!n) {
		n = new_node();
		n->name = xstrdup(c->name);
		node_add(mesh, n);
	} else {
		if(n->connection) {
			/* Oh dear, we already have a connection to this node. */
			logger(mesh, MESHLINK_INFO, "Established a second connection with %s, closing old connection", n->connection->name);

			if(n->connection->outgoing) {
				if(c->outgoing) {
					logger(mesh, MESHLINK_WARNING, "Two outgoing connections to the same node!");
				} else {
					c->outgoing = n->connection->outgoing;
				}

				n->connection->outgoing = NULL;
			}

			/* Remove the edge before terminating the connection, to prevent a graph update. */
			edge_del(mesh, n->connection->edge);
			n->connection->edge = NULL;

			terminate_connection(mesh, n->connection, false);
		}
	}

	n->devclass = devclass;
	n->status.dirty = true;
	n->status.tiny = c->flags & PROTOCOL_TINY;

	n->last_successfull_connection = mesh->loop.now.tv_sec;

	n->connection = c;
	n->nexthop = n;
	c->node = n;

	/* Activate this connection */

	c->allow_request = ALL;
	c->last_key_renewal = mesh->loop.now.tv_sec;
	c->status.active = true;

	logger(mesh, MESHLINK_INFO, "Connection with %s activated", c->name);

	if(mesh->meta_status_cb) {
		mesh->meta_status_cb(mesh, (meshlink_node_t *)n, true);
	}

	/*  Terminate any connections to this node that are not activated yet */

	for list_each(connection_t, other, mesh->connections) {
		if(!other->status.active && !strcmp(other->name, c->name)) {
			if(other->outgoing) {
				if(c->outgoing) {
					logger(mesh, MESHLINK_WARNING, "Two outgoing connections to the same node!");
				} else {
					c->outgoing = other->outgoing;
				}

				other->outgoing = NULL;
			}

			logger(mesh, MESHLINK_DEBUG, "Terminating pending second connection with %s", n->name);
			terminate_connection(mesh, other, false);
		}
	}

	/* Send him everything we know */

	if(!(c->flags & PROTOCOL_TINY)) {
		send_everything(mesh, c);
	}

	/* Create an edge_t for this connection */

	assert(devclass >= 0 && devclass < DEV_CLASS_COUNT);

	c->edge = new_edge();
	c->edge->from = mesh->self;
	c->edge->to = n;
	sockaddrcpy_setport(&c->edge->address, &c->address, atoi(hisport));
	c->edge->weight = mesh->dev_class_traits[devclass].edge_weight;
	c->edge->connection = c;

	node_add_recent_address(mesh, n, &c->address);
	edge_add(mesh, c->edge);

	/* Notify everyone of the new edge */

	send_add_edge(mesh, mesh->everyone, c->edge, 0);

	/* Run MST and SSSP algorithms */

	graph(mesh);

	/* Request a session key to jump start UDP traffic */

	if(c->status.initiator) {
		send_req_key(mesh, n);
	}

	return true;
}
