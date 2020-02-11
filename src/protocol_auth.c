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

static bool send_proxyrequest(meshlink_handle_t *mesh, connection_t *c) {
	switch(mesh->proxytype) {
	case PROXY_HTTP: {
		char *host;
		char *port;

		sockaddr2str(&c->address, &host, &port);
		send_request(mesh, c, NULL, "CONNECT %s:%s HTTP/1.1\r\n\r", host, port);
		free(host);
		free(port);
		return true;
	}

	case PROXY_SOCKS4: {
		if(c->address.sa.sa_family != AF_INET) {
			logger(mesh, MESHLINK_ERROR, "Cannot connect to an IPv6 host through a SOCKS 4 proxy!");
			return false;
		}

		char s4req[9 + (mesh->proxyuser ? strlen(mesh->proxyuser) : 0)];
		s4req[0] = 4;
		s4req[1] = 1;
		memcpy(s4req + 2, &c->address.in.sin_port, 2);
		memcpy(s4req + 4, &c->address.in.sin_addr, 4);

		if(mesh->proxyuser) {
			memcpy(s4req + 8, mesh->proxyuser, strlen(mesh->proxyuser));
		}

		s4req[sizeof(s4req) - 1] = 0;
		c->tcplen = 8;
		return send_meta(mesh, c, s4req, sizeof(s4req));
	}

	case PROXY_SOCKS5: {
		int len = 3 + 6 + (c->address.sa.sa_family == AF_INET ? 4 : 16);
		c->tcplen = 2;

		if(mesh->proxypass) {
			len += 3 + strlen(mesh->proxyuser) + strlen(mesh->proxypass);
		}

		char s5req[len];
		int i = 0;
		s5req[i++] = 5;
		s5req[i++] = 1;

		if(mesh->proxypass) {
			s5req[i++] = 2;
			s5req[i++] = 1;
			s5req[i++] = strlen(mesh->proxyuser);
			memcpy(s5req + i, mesh->proxyuser, strlen(mesh->proxyuser));
			i += strlen(mesh->proxyuser);
			s5req[i++] = strlen(mesh->proxypass);
			memcpy(s5req + i, mesh->proxypass, strlen(mesh->proxypass));
			i += strlen(mesh->proxypass);
			c->tcplen += 2;
		} else {
			s5req[i++] = 0;
		}

		s5req[i++] = 5;
		s5req[i++] = 1;
		s5req[i++] = 0;

		if(c->address.sa.sa_family == AF_INET) {
			s5req[i++] = 1;
			memcpy(s5req + i, &c->address.in.sin_addr, 4);
			i += 4;
			memcpy(s5req + i, &c->address.in.sin_port, 2);
			i += 2;
			c->tcplen += 10;
		} else if(c->address.sa.sa_family == AF_INET6) {
			s5req[i++] = 3;
			memcpy(s5req + i, &c->address.in6.sin6_addr, 16);
			i += 16;
			memcpy(s5req + i, &c->address.in6.sin6_port, 2);
			i += 2;
			c->tcplen += 22;
		} else {
			logger(mesh, MESHLINK_ERROR, "Address family %hx not supported for SOCKS 5 proxies!", c->address.sa.sa_family);
			return false;
		}

		if(i > len) {
			abort();
		}

		return send_meta(mesh, c, s5req, sizeof(s5req));
	}

	case PROXY_SOCKS4A:
		logger(mesh, MESHLINK_ERROR, "Proxy type not implemented yet");
		return false;

	default:
		logger(mesh, MESHLINK_ERROR, "Unknown proxy type");
		return false;
	}
}

bool send_id(meshlink_handle_t *mesh, connection_t *c) {
	if(mesh->proxytype && c->outgoing)
		if(!send_proxyrequest(mesh, c)) {
			return false;
		}

	return send_request(mesh, c, NULL, "%d %s %d.%d %s", ID, mesh->self->name, PROT_MAJOR, PROT_MINOR, mesh->appname);
}

static bool commit_invitation(meshlink_handle_t *mesh, connection_t *c, const void *data) {
	// Create a new node
	node_t *n = new_node();
	n->name = xstrdup(c->name);
	n->devclass = DEV_CLASS_UNKNOWN;
	n->ecdsa = ecdsa_set_public_key(data);
	n->submesh = c->submesh;

	// Remember its current address
	node_add_recent_address(mesh, n, &c->address);

	if(!node_write_config(mesh, n) || !config_sync(mesh, "current")) {
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

	if(!invitation_read(mesh, "current", cookie, &config, mesh->config_key)) {
		logger(mesh, MESHLINK_ERROR, "Error while trying to read invitation file\n");
		return false;
	}

	// Read the new node's Name from the file
	packmsg_input_t in = {config.buf, config.len};
	packmsg_get_uint32(&in); // skip version
	free(c->name);
	c->name = packmsg_get_str_dup(&in);

	// Check if the file contains Sub-Mesh information
	char *submesh_name = packmsg_get_str_dup(&in);

	if(!strcmp(submesh_name, CORE_MESH)) {
		free(submesh_name);
		c->submesh = NULL;
	} else {
		if(!check_id(submesh_name)) {
			logger(mesh, MESHLINK_ERROR, "Invalid invitation file %s\n", cookie);
			free(submesh_name);
			return false;
		}

		c->submesh = lookup_or_create_submesh(mesh, submesh_name);
		free(submesh_name);

		if(!c->submesh) {
			logger(mesh, MESHLINK_ERROR, "Unknown submesh in invitation file %s\n", cookie);
			return false;
		}
	}

	if(mesh->inviter_commits_first && !commit_invitation(mesh, c, (const char *)data + 18)) {
		return false;
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

	if(sscanf(request, "%*d " MAX_STRING " %d.%d", name, &c->protocol_major, &c->protocol_minor) < 2) {
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

	if(n->status.blacklisted) {
		logger(mesh, MESHLINK_WARNING, "Peer %s is blacklisted", c->name);
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
	char label[sizeof(meshlink_tcp_label) + strlen(mesh->self->name) + strlen(c->name) + 2];

	if(c->outgoing) {
		snprintf(label, sizeof(label), "%s %s %s", meshlink_tcp_label, mesh->self->name, c->name);
	} else {
		snprintf(label, sizeof(label), "%s %s %s", meshlink_tcp_label, c->name, mesh->self->name);
	}

	char buf1[1024], buf2[1024];
	bin2hex((uint8_t *)mesh->private_key + 64, buf1, 32);
	bin2hex((uint8_t *)n->ecdsa + 64, buf2, 32);
	logger(mesh, MESHLINK_DEBUG, "Connection to %s mykey %s hiskey %s", c->name, buf1, buf2);
	return sptps_start(&c->sptps, c, c->outgoing, false, mesh->private_key, n->ecdsa, label, sizeof(label) - 1, send_meta_sptps, receive_meta_sptps);
}

bool send_ack(meshlink_handle_t *mesh, connection_t *c) {
	return send_request(mesh, c, NULL, "%d %s %d %x", ACK, mesh->myport, mesh->devclass, OPTION_PMTU_DISCOVERY | (PROT_MINOR << 24));
}

static void send_everything(meshlink_handle_t *mesh, connection_t *c) {
	/* Send all known subnets and edges */

	for splay_each(node_t, n, mesh->nodes) {
		for splay_each(edge_t, e, n->edge_tree) {
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
			logger(mesh, MESHLINK_DEBUG, "Established a second connection with %s, closing old connection", n->connection->name);

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

	n->last_successfull_connection = mesh->loop.now.tv_sec;

	n->connection = c;
	c->node = n;

	/* Activate this connection */

	c->allow_request = ALL;
	c->status.active = true;

	logger(mesh, MESHLINK_INFO, "Connection with %s activated", c->name);

	/* Send him everything we know */

	send_everything(mesh, c);

	/* Create an edge_t for this connection */

	assert(devclass >= 0 && devclass < DEV_CLASS_COUNT);

	c->edge = new_edge();
	c->edge->from = mesh->self;
	c->edge->to = n;
	sockaddrcpy_setport(&c->edge->address, &c->address, atoi(hisport));
	c->edge->weight = mesh->dev_class_traits[devclass].edge_weight;
	c->edge->connection = c;

	edge_add(mesh, c->edge);

	/* Notify everyone of the new edge */

	send_add_edge(mesh, mesh->everyone, c->edge, 0);

	/* Run MST and SSSP algorithms */

	graph(mesh);

	return true;
}
