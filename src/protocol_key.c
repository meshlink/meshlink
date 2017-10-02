/*
    protocol_key.c -- handle the meta-protocol, key exchange
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

#include "connection.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "net.h"
#include "netutl.h"
#include "node.h"
#include "prf.h"
#include "protocol.h"
#include "sptps.h"
#include "utils.h"
#include "xalloc.h"

void send_key_changed(meshlink_handle_t *mesh) {
	send_request(mesh, mesh->everyone, "%d %x %s", KEY_CHANGED, rand(), mesh->self->name);

	/* Force key exchange for connections using SPTPS */

	for splay_each(node_t, n, mesh->nodes)
		if(n->status.reachable && n->status.validkey)
			sptps_force_kex(&n->sptps);
}

bool key_changed_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	char name[MAX_STRING_SIZE];
	node_t *n;

	if(sscanf(request, "%*d %*x " MAX_STRING, name) != 1) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "KEY_CHANGED", c->name);
		return false;
	}

	if(seen_request(mesh, request))
		return true;

	n = lookup_node(mesh, name);

	if(!n) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s origin %s which does not exist", "KEY_CHANGED", c->name, name);
		return true;
	}

	/* Tell the others */

	forward_request(mesh, c, request);

	return true;
}

static bool send_initial_sptps_data(void *handle, uint8_t type, const void *data, size_t len) {
	(void)type;
	node_t *to = handle;
	meshlink_handle_t *mesh = to->mesh;
	to->sptps.send_data = send_sptps_data;
	char buf[len * 4 / 3 + 5];
	b64encode(data, buf, len);
	return send_request(mesh, to->nexthop->connection, "%d %s %s %d %s", REQ_KEY, mesh->self->name, to->name, REQ_KEY, buf);
}

bool send_req_key(meshlink_handle_t *mesh, node_t *to) {
	if(!node_read_ecdsa_public_key(mesh, to)) {
		logger(mesh, MESHLINK_DEBUG, "No ECDSA key known for %s", to->name);
		send_request(mesh, to->nexthop->connection, "%d %s %s %d", REQ_KEY, mesh->self->name, to->name, REQ_PUBKEY);
		return true;
	}

	if(to->sptps.label)
		logger(mesh, MESHLINK_DEBUG, "send_req_key(%s) called while sptps->label != NULL!", to->name);

	char label[sizeof(meshlink_udp_label) + strlen(mesh->self->name) + strlen(to->name) + 2];
	snprintf(label, sizeof(label), "%s %s %s", meshlink_udp_label, mesh->self->name, to->name);
	sptps_stop(&to->sptps);
	to->status.validkey = false;
	to->status.waitingforkey = true;
	to->last_req_key = mesh->loop.now.tv_sec;
	to->incompression = mesh->self->incompression;
	return sptps_start(&to->sptps, to, true, true, mesh->self->connection->ecdsa, to->ecdsa, label, sizeof(label) - 1, send_initial_sptps_data, receive_sptps_record);
}

/* REQ_KEY is overloaded to allow arbitrary requests to be routed between two nodes. */

static bool req_key_ext_h(meshlink_handle_t *mesh, connection_t *c, const char *request, node_t *from, int reqno) {
	(void)c;
	switch(reqno) {
	case REQ_PUBKEY: {
		char *pubkey = ecdsa_get_base64_public_key(mesh->self->connection->ecdsa);
		send_request(mesh, from->nexthop->connection, "%d %s %s %d %s", REQ_KEY, mesh->self->name, from->name, ANS_PUBKEY, pubkey);
		free(pubkey);
		return true;
	}

	case ANS_PUBKEY: {
		if(node_read_ecdsa_public_key(mesh, from)) {
			logger(mesh, MESHLINK_WARNING, "Got ANS_PUBKEY from %s even though we already have his pubkey", from->name);
			return true;
		}

		char pubkey[MAX_STRING_SIZE];
		if(sscanf(request, "%*d %*s %*s %*d " MAX_STRING, pubkey) != 1 || !(from->ecdsa = ecdsa_set_base64_public_key(pubkey))) {
			logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ANS_PUBKEY", from->name, "invalid pubkey");
			return true;
		}

		logger(mesh, MESHLINK_INFO, "Learned ECDSA public key from %s", from->name);
		append_config_file(mesh, from->name, "ECDSAPublicKey", pubkey);
		return true;
	}

	case REQ_KEY: {
		if(!node_read_ecdsa_public_key(mesh, from)) {
			logger(mesh, MESHLINK_DEBUG, "No ECDSA key known for %s", from->name);
			send_request(mesh, from->nexthop->connection, "%d %s %s %d", REQ_KEY, mesh->self->name, from->name, REQ_PUBKEY);
			return true;
		}

		if(from->sptps.label) {
			logger(mesh, MESHLINK_DEBUG, "Got REQ_KEY from %s while we already started a SPTPS session!", from->name);
			if(strcmp(mesh->self->name, from->name) < 0) {
				logger(mesh, MESHLINK_DEBUG, "Ignoring REQ_KEY from %s.", from->name);
				return true;
			}
		}

		char buf[MAX_STRING_SIZE];
		int len;

		if(sscanf(request, "%*d %*s %*s %*d " MAX_STRING, buf) != 1 || !(len = b64decode(buf, buf, strlen(buf)))) {
			logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "REQ_SPTPS_START", from->name, "invalid SPTPS data");
			return true;
		}

		char label[sizeof(meshlink_udp_label) + strlen(from->name) + strlen(mesh->self->name) + 2];
		snprintf(label, sizeof(label), "%s %s %s", meshlink_udp_label, from->name, mesh->self->name);
		sptps_stop(&from->sptps);
		from->status.validkey = false;
		from->status.waitingforkey = true;
		from->last_req_key = mesh->loop.now.tv_sec;
		sptps_start(&from->sptps, from, false, true, mesh->self->connection->ecdsa, from->ecdsa, label, sizeof(label) - 1, send_sptps_data, receive_sptps_record);
		sptps_receive_data(&from->sptps, buf, len);
		return true;
	}

	case REQ_SPTPS: {
		if(!from->status.validkey) {
			logger(mesh, MESHLINK_ERROR, "Got REQ_SPTPS from %s but we don't have a valid key yet", from->name);
			return true;
		}

		char buf[MAX_STRING_SIZE];
		int len;
		if(sscanf(request, "%*d %*s %*s %*d " MAX_STRING, buf) != 1 || !(len = b64decode(buf, buf, strlen(buf)))) {
			logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "REQ_SPTPS", from->name, "invalid SPTPS data");
			return true;
		}
		sptps_receive_data(&from->sptps, buf, len);
		return true;
	}

	default:
		logger(mesh, MESHLINK_ERROR, "Unknown extended REQ_KEY request from %s: %s", from->name, request);
		return true;
	}
}

bool req_key_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	char from_name[MAX_STRING_SIZE];
	char to_name[MAX_STRING_SIZE];
	node_t *from, *to;
	int reqno = 0;

	if(sscanf(request, "%*d " MAX_STRING " " MAX_STRING " %d", from_name, to_name, &reqno) < 2) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "REQ_KEY", c->name);
		return false;
	}

	if(!check_id(from_name) || !check_id(to_name)) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "REQ_KEY", c->name, "invalid name");
		return false;
	}

	from = lookup_node(mesh, from_name);

	if(!from) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s origin %s which does not exist in our connection list",
		       "REQ_KEY", c->name, from_name);
		return true;
	}

	to = lookup_node(mesh, to_name);

	if(!to) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s destination %s which does not exist in our connection list",
		       "REQ_KEY", c->name, to_name);
		return true;
	}

	/* Check if this key request is for us */

	if(to == mesh->self) {                      /* Yes */
		/* Is this an extended REQ_KEY message? */
		if(reqno)
			return req_key_ext_h(mesh, c, request, from, reqno);

		/* This should never happen. Ignore it, unless it came directly from the connected peer, in which case we disconnect. */
		return from->connection != c;
	} else {
		if(!to->status.reachable) {
			logger(mesh, MESHLINK_WARNING, "Got %s from %s destination %s which is not reachable",
			       "REQ_KEY", c->name, to_name);
			return true;
		}

		send_request(mesh, to->nexthop->connection, "%s", request);
	}

	return true;
}

bool send_ans_key(meshlink_handle_t *mesh, node_t *to) {
	(void)mesh;
	(void)to;
	abort();
}

bool ans_key_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	char from_name[MAX_STRING_SIZE];
	char to_name[MAX_STRING_SIZE];
	char key[MAX_STRING_SIZE];
	char address[MAX_STRING_SIZE] = "";
	char port[MAX_STRING_SIZE] = "";
	int cipher, digest, maclength, compression;
	node_t *from, *to;

	if(sscanf(request, "%*d "MAX_STRING" "MAX_STRING" "MAX_STRING" %d %d %d %d "MAX_STRING" "MAX_STRING,
	                from_name, to_name, key, &cipher, &digest, &maclength,
	                &compression, address, port) < 7) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "ANS_KEY", c->name);
		return false;
	}

	if(!check_id(from_name) || !check_id(to_name)) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ANS_KEY", c->name, "invalid name");
		return false;
	}

	from = lookup_node(mesh, from_name);

	if(!from) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s origin %s which does not exist in our connection list",
		       "ANS_KEY", c->name, from_name);
		return true;
	}

	to = lookup_node(mesh, to_name);

	if(!to) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s destination %s which does not exist in our connection list",
		       "ANS_KEY", c->name, to_name);
		return true;
	}

	/* Forward it if necessary */

	if(to != mesh->self) {
		if(!to->status.reachable) {
			logger(mesh, MESHLINK_WARNING, "Got %s from %s destination %s which is not reachable",
			       "ANS_KEY", c->name, to_name);
			return true;
		}

		if(!*address && from->address.sa.sa_family != AF_UNSPEC) {
			char *address, *port;
			logger(mesh, MESHLINK_DEBUG, "Appending reflexive UDP address to ANS_KEY from %s to %s", from->name, to->name);
			sockaddr2str(&from->address, &address, &port);
			send_request(mesh, to->nexthop->connection, "%s %s %s", request, address, port);
			free(address);
			free(port);
			return true;
		}

		return send_request(mesh, to->nexthop->connection, "%s", request);
	}

	/* Don't use key material until every check has passed. */
	from->status.validkey = false;

	/* Compression is not supported. */
	if(compression != 0) {
		logger(mesh, MESHLINK_ERROR, "Node %s uses bogus compression level!", from->name);
		return true;
	}

	from->outcompression = compression;

	/* SPTPS or old-style key exchange? */

	char buf[strlen(key)];
	int len = b64decode(key, buf, strlen(key));

	if(!len || !sptps_receive_data(&from->sptps, buf, len))
		logger(mesh, MESHLINK_ERROR, "Error processing SPTPS data from %s", from->name);

	if(from->status.validkey) {
		if(*address && *port) {
			logger(mesh, MESHLINK_DEBUG, "Using reflexive UDP address from %s: %s port %s", from->name, address, port);
			sockaddr_t sa = str2sockaddr(address, port);
			update_node_udp(mesh, from, &sa);
		}

		if(from->options & OPTION_PMTU_DISCOVERY && !(from->options & OPTION_TCPONLY))
			send_mtu_probe(mesh, from);
	}

	return true;
}
