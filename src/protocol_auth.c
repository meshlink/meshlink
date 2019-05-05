/*
    protocol_auth.c -- handle the meta-protocol, authentication
    Copyright (C) 2014-2019 Guus Sliepen <guus@meshlink.io>

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

	int minor = mesh->self->connection->protocol_minor;

	if(mesh->proxytype && c->outgoing)
		if(!send_proxyrequest(mesh, c)) {
			return false;
		}

	return send_request(mesh, c, NULL, "%d %s %d.%d %s", ID, mesh->self->connection->name, mesh->self->connection->protocol_major, minor, mesh->appname);
}

static bool finalize_invitation(meshlink_handle_t *mesh, connection_t *c, const void *data, uint16_t len) {
	(void)len;

	if(strchr(data, '\n')) {
		logger(mesh, MESHLINK_ERROR, "Received invalid key from invited node %s!\n", c->name);
		return false;
	}

	// Create a new host config file
	char filename[PATH_MAX];
	snprintf(filename, sizeof(filename), "%s" SLASH "hosts" SLASH "%s", mesh->confbase, c->name);

	if(!access(filename, F_OK)) {
		logger(mesh, MESHLINK_ERROR, "Host config file for %s already exists!\n", c->name);
		return false;
	}

	FILE *f = fopen(filename, "w");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Error trying to create %s: %s\n", filename, strerror(errno));
		return false;
	}

	fprintf(f, "ECDSAPublicKey = %s\n", (const char *)data);

	if(c->submesh) {
		fprintf(f, "SubMesh = %s\n", c->submesh->name);
	}

	fclose(f);

	logger(mesh, MESHLINK_INFO, "Key successfully received from %s", c->name);

	//TODO: callback to application to inform of an accepted invitation

	sptps_send_record(&c->sptps, 2, data, 0);

	load_all_nodes(mesh);

	return true;
}

static bool receive_invitation_sptps(void *handle, uint8_t type, const void *data, uint16_t len) {
	connection_t *c = handle;
	meshlink_handle_t *mesh = c->mesh;

	if(type == 128) {
		return true;
	}

	if(type == 1 && c->status.invitation_used) {
		return finalize_invitation(mesh, c, data, len);
	}

	if(type != 0 || len != 18 || c->status.invitation_used) {
		return false;
	}

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

	char filename[PATH_MAX], usedname[PATH_MAX];
	snprintf(filename, sizeof(filename), "%s" SLASH "invitations" SLASH "%s", mesh->confbase, cookie);
	snprintf(usedname, sizeof(usedname), "%s" SLASH "invitations" SLASH "%s.used", mesh->confbase, cookie);

	// Atomically rename the invitation file
	if(rename(filename, usedname)) {
		if(errno == ENOENT) {
			logger(mesh, MESHLINK_ERROR, "Peer %s tried to use non-existing invitation %s\n", c->name, cookie);
		} else {
			logger(mesh, MESHLINK_ERROR, "Error trying to rename invitation %s\n", cookie);
		}

		return false;
	}

	// Open the renamed file
	FILE *f = fopen(usedname, "r");

	if(!f) {
		logger(mesh, MESHLINK_ERROR, "Error trying to open invitation %s\n", cookie);
		unlink(usedname);
		return false;
	}

	// Check the timestamp
	struct stat st;

	if(fstat(fileno(f), &st)) {
		logger(mesh, MESHLINK_ERROR, "Could not stat invitation file %s\n", usedname);
		fclose(f);
		unlink(usedname);
		return false;
	}

	if(time(NULL) > st.st_mtime + mesh->invitation_timeout) {
		logger(mesh, MESHLINK_ERROR, "Peer %s tried to use an outdated invitation file %s\n", c->name, usedname);
		fclose(f);
		unlink(usedname);
		return false;
	}

	// Read the new node's Name from the file
	char buf[1024];
	fgets(buf, sizeof(buf), f);

	if(*buf) {
		buf[strlen(buf) - 1] = 0;
	}

	len = strcspn(buf, " \t=");
	char *name = buf + len;
	name += strspn(name, " \t");

	if(*name == '=') {
		name++;
		name += strspn(name, " \t");
	}

	buf[len] = 0;

	if(!*buf || !*name || strcasecmp(buf, "Name") || !check_id(name)) {
		logger(mesh, MESHLINK_ERROR, "Invalid invitation file %s\n", cookie);
		fclose(f);
		return false;
	}

	free(c->name);
	c->name = xstrdup(name);

	// Check if the file contains Sub-Mesh information
	buf[0] = 0;
	fgets(buf, sizeof(buf), f);

	if(*buf) {
		buf[strlen(buf) - 1] = 0;
	}

	if(!strncmp(buf, "SubMesh", 7)) {
		len = strcspn(buf, " \t=");
		char *submesh_name = buf + len;
		submesh_name += strspn(submesh_name, " \t");

		if(*submesh_name == '=') {
			submesh_name++;
			submesh_name += strspn(submesh_name, " \t");
		}

		if(!check_id(submesh_name)) {
			logger(mesh, MESHLINK_ERROR, "Invalid invitation file %s\n", cookie);
			fclose(f);
			return false;
		}

		c->submesh = NULL;
		c->submesh = lookup_or_create_submesh(mesh, submesh_name);

		if(!c->submesh) {
			return false;
		}
	}

	// Send the node the contents of the invitation file
	rewind(f);
	size_t result;

	while((result = fread(buf, 1, sizeof(buf), f))) {
		sptps_send_record(&c->sptps, 0, buf, result);
	}

	sptps_send_record(&c->sptps, 1, buf, 0);
	fclose(f);
	unlink(usedname);

	c->status.invitation_used = true;

	logger(mesh, MESHLINK_INFO, "Invitation %s successfully sent to %s", cookie, c->name);
	return true;
}

bool id_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
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

	if(c->protocol_major != mesh->self->connection->protocol_major) {
		logger(mesh, MESHLINK_ERROR, "Peer %s uses incompatible version %d.%d",
		       c->name, c->protocol_major, c->protocol_minor);
		return false;
	}

	if(!c->config_tree) {
		init_configuration(&c->config_tree);

		if(!read_host_config(mesh, c->config_tree, c->name)) {
			logger(mesh, MESHLINK_ERROR, "Peer %s has unknown identity", c->name);
			return false;
		}
	}

	bool blacklisted = false;
	get_config_bool(lookup_config(c->config_tree, "blacklisted"), &blacklisted);

	if(blacklisted) {
		logger(mesh, MESHLINK_EPEER, "Peer %s is blacklisted", c->name);
		return false;
	}

	read_ecdsa_public_key(mesh, c);

	if(!ecdsa_active(c->ecdsa)) {
		logger(mesh, MESHLINK_ERROR, "No key known for peer %s", c->name);

		node_t *n = lookup_node(mesh, c->name);

		if(n && n->status.reachable && !n->status.waitingforkey) {
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

	return sptps_start(&c->sptps, c, c->outgoing, false, mesh->self->connection->ecdsa, c->ecdsa, label, sizeof(label) - 1, send_meta_sptps, receive_meta_sptps);
}

bool send_ack(meshlink_handle_t *mesh, connection_t *c) {

	/* Check some options */

	if(mesh->self->options & OPTION_PMTU_DISCOVERY) {
		c->options |= OPTION_PMTU_DISCOVERY;
	}

	return send_request(mesh, c, NULL, "%d %s %d %x", ACK, mesh->myport, mesh->devclass, (c->options & 0xffffff) | (PROT_MINOR << 24));
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
	char hisport[MAX_STRING_SIZE];
	char *hisaddress;
	int devclass;
	uint32_t options;
	node_t *n;

	if(sscanf(request, "%*d " MAX_STRING " %d %x", hisport, &devclass, &options) != 3) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "ACK", c->name);
		return false;
	}

	if(devclass < 0 || devclass > _DEV_CLASS_MAX) {
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

			terminate_connection(mesh, n->connection, false);
		}
	}

	n->devclass = devclass;
	node_write_devclass(mesh, n);

	n->last_successfull_connection = time(NULL);

	n->connection = c;
	c->node = n;

	if(!(c->options & options & OPTION_PMTU_DISCOVERY)) {
		c->options &= ~OPTION_PMTU_DISCOVERY;
		options &= ~OPTION_PMTU_DISCOVERY;
	}

	c->options |= options;

	/* Activate this connection */

	c->allow_request = ALL;
	c->status.active = true;

	logger(mesh, MESHLINK_INFO, "Connection with %s activated", c->name);

	/* Send him everything we know */

	send_everything(mesh, c);

	/* Create an edge_t for this connection */

	assert(devclass >= 0 && devclass <= _DEV_CLASS_MAX);

	c->edge = new_edge();
	c->edge->from = mesh->self;
	c->edge->to = n;
	sockaddr2str(&c->address, &hisaddress, NULL);
	c->edge->address = str2sockaddr(hisaddress, hisport);
	free(hisaddress);
	c->edge->weight = dev_class_traits[devclass].edge_weight;
	c->edge->connection = c;
	c->edge->options = c->options;

	edge_add(mesh, c->edge);

	/* Notify everyone of the new edge */

	send_add_edge(mesh, mesh->everyone, c->edge, 0);

	/* Run MST and SSSP algorithms */

	graph_add_edge(mesh, c->edge);

	return true;
}
