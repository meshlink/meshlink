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

static const int req_key_timeout = 2;

bool key_changed_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	assert(request);
	assert(*request);

	char name[MAX_STRING_SIZE];
	node_t *n;

	if(sscanf(request, "%*d %*x " MAX_STRING, name) != 1) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s", "KEY_CHANGED", c->name);
		return false;
	}

	if(seen_request(mesh, request)) {
		return true;
	}

	n = lookup_node(mesh, name);

	if(!n) {
		logger(mesh, MESHLINK_ERROR, "Got %s from %s origin %s which does not exist", "KEY_CHANGED", c->name, name);
		return true;
	}

	/* Tell the others */

	forward_request(mesh, c, NULL, request);

	return true;
}

static bool send_initial_sptps_data(void *handle, uint8_t type, const void *data, size_t len) {
	(void)type;

	assert(data);
	assert(len);

	node_t *to = handle;
	meshlink_handle_t *mesh = to->mesh;

	if(!to->nexthop || !to->nexthop->connection) {
		logger(mesh, MESHLINK_WARNING, "Cannot send SPTPS data to %s via %s", to->name, to->nexthop ? to->nexthop->name : to->name);
		return false;
	}

	to->sptps.send_data = send_sptps_data;
	char buf[len * 4 / 3 + 5];
	b64encode(data, buf, len);
	return send_request(mesh, to->nexthop->connection, NULL, "%d %s %s %d %s", REQ_KEY, mesh->self->name, to->name, REQ_KEY, buf);
}

bool send_external_ip_address(meshlink_handle_t *mesh, node_t *to) {
	if(!mesh->self->external_ip_address) {
		return true;
	}

	return send_request(mesh, to->nexthop->connection, NULL, "%d %s %s %d %s %s", REQ_KEY, mesh->self->name, to->name, REQ_EXTERNAL, mesh->self->external_ip_address, mesh->myport);
}

bool send_canonical_address(meshlink_handle_t *mesh, node_t *to) {
	if(!mesh->self->canonical_address) {
		return true;
	}

	return send_request(mesh, to->nexthop->connection, NULL, "%d %s %s %d %s", REQ_KEY, mesh->self->name, to->name, REQ_CANONICAL, mesh->self->canonical_address);
}

bool send_req_key(meshlink_handle_t *mesh, node_t *to) {
	if(!node_read_public_key(mesh, to)) {
		logger(mesh, MESHLINK_DEBUG, "No ECDSA key known for %s", to->name);

		if(!to->nexthop || !to->nexthop->connection) {
			logger(mesh, MESHLINK_WARNING, "Cannot send REQ_PUBKEY to %s via %s", to->name, to->nexthop ? to->nexthop->name : to->name);
			return true;
		}

		char *pubkey = ecdsa_get_base64_public_key(mesh->private_key);
		send_request(mesh, to->nexthop->connection, NULL, "%d %s %s %d %s", REQ_KEY, mesh->self->name, to->name, REQ_PUBKEY, pubkey);
		free(pubkey);
		return true;
	}

	if(to->sptps.label) {
		logger(mesh, MESHLINK_DEBUG, "send_req_key(%s) called while sptps->label != NULL!", to->name);
	}

	/* Send our canonical address to help with UDP hole punching */
	send_canonical_address(mesh, to);

	/* Send our external IP address to help with UDP hole punching */
	send_external_ip_address(mesh, to);

	char label[sizeof(meshlink_udp_label) + strlen(mesh->self->name) + strlen(to->name) + 2];
	snprintf(label, sizeof(label), "%s %s %s", meshlink_udp_label, mesh->self->name, to->name);
	sptps_stop(&to->sptps);
	to->status.validkey = false;
	to->status.waitingforkey = true;
	to->last_req_key = mesh->loop.now.tv_sec;
	return sptps_start(&to->sptps, to, true, true, mesh->private_key, to->ecdsa, label, sizeof(label) - 1, send_initial_sptps_data, receive_sptps_record);
}

/* REQ_KEY is overloaded to allow arbitrary requests to be routed between two nodes. */

static bool req_key_ext_h(meshlink_handle_t *mesh, connection_t *c, const char *request, node_t *from, int reqno) {
	(void)c;

	if(!from->nexthop || !from->nexthop->connection) {
		logger(mesh, MESHLINK_WARNING, "Cannot answer REQ_KEY from %s via %s", from->name, from->nexthop ? from->nexthop->name : from->name);
		return true;
	}

	switch(reqno) {
	case REQ_PUBKEY: {
		char *pubkey = ecdsa_get_base64_public_key(mesh->private_key);

		if(!node_read_public_key(mesh, from)) {
			char hiskey[MAX_STRING_SIZE];

			if(sscanf(request, "%*d %*s %*s %*d " MAX_STRING, hiskey) == 1) {
				from->ecdsa = ecdsa_set_base64_public_key(hiskey);

				if(!from->ecdsa) {
					logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "REQ_PUBKEY", from->name, "invalid pubkey");
					return true;
				}

				logger(mesh, MESHLINK_INFO, "Learned ECDSA public key from %s", from->name);
				from->status.dirty = true;

				if(!node_write_config(mesh, from, true)) {
					// ignore
				}
			}
		}

		send_request(mesh, from->nexthop->connection, NULL, "%d %s %s %d %s", REQ_KEY, mesh->self->name, from->name, ANS_PUBKEY, pubkey);
		free(pubkey);
		return true;
	}

	case ANS_PUBKEY: {
		if(node_read_public_key(mesh, from)) {
			logger(mesh, MESHLINK_WARNING, "Got ANS_PUBKEY from %s even though we already have his pubkey", from->name);
			return true;
		}

		char pubkey[MAX_STRING_SIZE];

		if(sscanf(request, "%*d %*s %*s %*d " MAX_STRING, pubkey) != 1 || !(from->ecdsa = ecdsa_set_base64_public_key(pubkey))) {
			logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "ANS_PUBKEY", from->name, "invalid pubkey");
			return true;
		}

		logger(mesh, MESHLINK_INFO, "Learned ECDSA public key from %s", from->name);
		from->status.dirty = true;

		if(!node_write_config(mesh, from, true)) {
			// ignore
		}

		/* If we are trying to form an outgoing connection to this node, retry immediately */
		for list_each(outgoing_t, outgoing, mesh->outgoings) {
			if(outgoing->node == from && outgoing->ev.cb) {
				outgoing->timeout = 0;
				timeout_set(&mesh->loop, &outgoing->ev, &(struct timespec) {
					0, 0
				});
			}
		}

		/* Also reset any UTCP timers */
		utcp_reset_timers(from->utcp);

		return true;
	}

	case REQ_KEY: {
		if(!node_read_public_key(mesh, from)) {
			logger(mesh, MESHLINK_DEBUG, "No ECDSA key known for %s", from->name);
			send_request(mesh, from->nexthop->connection, NULL, "%d %s %s %d", REQ_KEY, mesh->self->name, from->name, REQ_PUBKEY);
			return true;
		}

		if(from->sptps.label) {
			logger(mesh, MESHLINK_DEBUG, "Got REQ_KEY from %s while we already started a SPTPS session!", from->name);

			if(mesh->loop.now.tv_sec < from->last_req_key + req_key_timeout && strcmp(mesh->self->name, from->name) < 0) {
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

		/* Send our canonical address to help with UDP hole punching */
		send_canonical_address(mesh, from);

		/* Send our external IP address to help with UDP hole punching */
		send_external_ip_address(mesh, from);

		if(!sptps_start(&from->sptps, from, false, true, mesh->private_key, from->ecdsa, label, sizeof(label) - 1, send_sptps_data, receive_sptps_record)) {
			logger(mesh, MESHLINK_ERROR, "Could not start SPTPS session with %s: %s", from->name, strerror(errno));
			return true;
		}

		if(!sptps_receive_data(&from->sptps, buf, len)) {
			logger(mesh, MESHLINK_ERROR, "Could not process SPTPS data from %s: %s", from->name, strerror(errno));
			return true;
		}

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

		if(!sptps_receive_data(&from->sptps, buf, len)) {
			logger(mesh, MESHLINK_ERROR, "Could not process SPTPS data from %s: %s", from->name, strerror(errno));
			return true;
		}

		return true;
	}

	case REQ_CANONICAL: {
		char host[MAX_STRING_SIZE];
		char port[MAX_STRING_SIZE];

		if(sscanf(request, "%*d %*s %*s %*d " MAX_STRING " " MAX_STRING, host, port) != 2) {
			logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "REQ_CANONICAL", from->name, "invalid canonical address");
			return true;
		}

		char *canonical_address;
		xasprintf(&canonical_address, "%s %s", host, port);

		if(mesh->log_level <= MESHLINK_DEBUG && (!from->canonical_address || strcmp(from->canonical_address, canonical_address))) {
			logger(mesh, MESHLINK_DEBUG, "Updating canonical address of %s to %s", from->name, canonical_address);
		}

		free(from->canonical_address);
		from->canonical_address = canonical_address;
		return true;
	}

	case REQ_EXTERNAL: {
		char ip[MAX_STRING_SIZE];
		char port[MAX_STRING_SIZE];
		logger(mesh, MESHLINK_DEBUG, "Got %s from %s with data: %s", "REQ_EXTERNAL", from->name, request);

		if(sscanf(request, "%*d %*s %*s %*d " MAX_STRING " " MAX_STRING, ip, port) != 2) {
			logger(mesh, MESHLINK_ERROR, "Got bad %s from %s: %s", "REQ_EXTERNAL", from->name, request);
			return true;
		}

		char *external_ip_address;
		xasprintf(&external_ip_address, "%s %s", ip, port);

		if(mesh->log_level <= MESHLINK_DEBUG && (!from->external_ip_address || strcmp(from->external_ip_address, external_ip_address))) {
			logger(mesh, MESHLINK_DEBUG, "Updating external IP address of %s to %s", from->name, external_ip_address);
		}

		free(from->external_ip_address);
		from->external_ip_address = external_ip_address;
		return true;
	}

	default:
		logger(mesh, MESHLINK_ERROR, "Unknown extended REQ_KEY request from %s: %s", from->name, request);
		return true;
	}
}

bool req_key_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	assert(request);
	assert(*request);

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
		if(reqno) {
			return req_key_ext_h(mesh, c, request, from, reqno);
		}

		/* This should never happen. Ignore it, unless it came directly from the connected peer, in which case we disconnect. */
		return from->connection != c;
	} else {
		if(!to->status.reachable || !to->nexthop || !to->nexthop->connection) {
			logger(mesh, MESHLINK_WARNING, "Got %s from %s destination %s which is not reachable",
			       "REQ_KEY", c->name, to_name);
			return true;
		}

		size_t len = strlen(request);
		from->in_forward += len + SPTPS_OVERHEAD;
		to->out_forward += len + SPTPS_OVERHEAD;

		send_request(mesh, to->nexthop->connection, NULL, "%s", request);
	}

	return true;
}

bool ans_key_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	assert(request);
	assert(*request);

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

		if(from == to) {
			logger(mesh, MESHLINK_WARNING, "Got %s from %s from %s to %s",
			       "ANS_KEY", c->name, from_name, to_name);
			return true;
		}

		if(!to->nexthop || !to->nexthop->connection) {
			logger(mesh, MESHLINK_WARNING, "Cannot forward ANS_KEY to %s via %s", to->name, to->nexthop ? to->nexthop->name : to->name);
			return false;
		}

		/* TODO: find a good way to avoid the use of strlen() */
		size_t len = strlen(request);
		from->in_forward += len + SPTPS_OVERHEAD;
		to->out_forward += len + SPTPS_OVERHEAD;

		/* Append the known UDP address of the from node, if we have a confirmed one */
		if(!*address && from->status.udp_confirmed && from->address.sa.sa_family != AF_UNSPEC) {
			char *reflexive_address, *reflexive_port;
			logger(mesh, MESHLINK_DEBUG, "Appending reflexive UDP address to ANS_KEY from %s to %s", from->name, to->name);
			sockaddr2str(&from->address, &reflexive_address, &reflexive_port);
			send_request(mesh, to->nexthop->connection, NULL, "%s %s %s", request, reflexive_address, reflexive_port);
			free(reflexive_address);
			free(reflexive_port);
			return true;
		}

		return send_request(mesh, to->nexthop->connection, NULL, "%s", request);
	}

	/* Is this an ANS_KEY informing us of our own reflexive UDP address? */

	if(from == mesh->self) {
		if(*key == '.' && *address && *port) {
			logger(mesh, MESHLINK_DEBUG, "Learned our own reflexive UDP address from %s: %s port %s", c->name, address, port);

			/* Inform all other nodes we want to communicate with and which are reachable via this connection */
			for splay_each(node_t, n, mesh->nodes) {
				if(n->nexthop != c->node) {
					continue;
				}

				if(n->status.udp_confirmed) {
					continue;
				}

				if(!n->status.waitingforkey && !n->status.validkey) {
					continue;
				}

				if(!n->nexthop->connection) {
					continue;
				}

				logger(mesh, MESHLINK_DEBUG, "Forwarding our own reflexive UDP address to %s", n->name);
				send_request(mesh, c, NULL, "%d %s %s . -1 -1 -1 0 %s %s", ANS_KEY, mesh->self->name, n->name, address, port);
			}
		} else {
			logger(mesh, MESHLINK_WARNING, "Got %s from %s from %s to %s",
			       "ANS_KEY", c->name, from_name, to_name);
		}

		return true;
	}

	/* Process SPTPS data if present */

	if(*key != '.') {
		/* Don't use key material until every check has passed. */
		from->status.validkey = false;

		/* Compression is not supported. */
		if(compression != 0) {
			logger(mesh, MESHLINK_ERROR, "Node %s uses bogus compression level!", from->name);
			return true;
		}

		char buf[strlen(key)];
		int len = b64decode(key, buf, strlen(key));

		if(!len || !sptps_receive_data(&from->sptps, buf, len)) {
			logger(mesh, MESHLINK_ERROR, "Error processing SPTPS data from %s", from->name);
		}
	}

	if(from->status.validkey) {
		if(*address && *port) {
			logger(mesh, MESHLINK_DEBUG, "Using reflexive UDP address from %s: %s port %s", from->name, address, port);
			sockaddr_t sa = str2sockaddr(address, port);
			update_node_udp(mesh, from, &sa);
		}

		send_mtu_probe(mesh, from);
	}

	return true;
}
