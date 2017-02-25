/*
    protocol.c -- handle the meta-protocol, basic functions
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>

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
#include "logger.h"
#include "meshlink_internal.h"
#include "meta.h"
#include "protocol.h"
#include "utils.h"
#include "xalloc.h"

/* Jumptable for the request handlers */

static bool (*request_handlers[])(meshlink_handle_t *, connection_t *, const char *) = {
		id_h, NULL, NULL, NULL /* metakey_h, challenge_h, chal_reply_h */, ack_h,
		status_h, error_h, termreq_h,
		ping_h, pong_h,
		NULL, NULL, //add_subnet_h, del_subnet_h,
		add_edge_h, del_edge_h,
		key_changed_h, req_key_h, ans_key_h, tcppacket_h, NULL, //control_h,
};

/* Request names */

static char (*request_name[]) = {
		"ID", "METAKEY", "CHALLENGE", "CHAL_REPLY", "ACK",
		"STATUS", "ERROR", "TERMREQ",
		"PING", "PONG",
		"ADD_SUBNET", "DEL_SUBNET",
		"ADD_EDGE", "DEL_EDGE", "KEY_CHANGED", "REQ_KEY", "ANS_KEY", "PACKET", "CONTROL",
};

bool check_id(const char *id) {
	if(!id || !*id)
		return false;

	for(; *id; id++)
		if(!isalnum(*id) && *id != '_')
			return false;

	return true;
}

/* Generic request routines - takes care of logging and error
   detection as well */

// @return the sockerrno, 0 on success, -1 on other errors
int send_request(meshlink_handle_t *mesh, connection_t *c, const char *format, ...) {
	if( !c ) {
		logger(mesh, MESHLINK_ERROR, "Can't send request to nullified connection.");
		return -1;
	}

	va_list args;
	char request[MAXBUFSIZE];
	int len;

	/* Use vsnprintf instead of vxasprintf: faster, no memory
	   fragmentation, cleanup is automatic, and there is a limit on the
	   input buffer anyway */

	va_start(args, format);
	len = vsnprintf(request, MAXBUFSIZE, format, args);
	va_end(args);

	if(len < 0 || len > MAXBUFSIZE - 1) {
		logger(mesh, MESHLINK_ERROR, "Output buffer overflow while sending request to %s (%s)",
			   c->name, c->hostname);
		return -1;
	}

	logger(mesh, MESHLINK_DEBUG, "Sending %s to %s (%s): %s", request_name[atoi(request)], c->name, c->hostname, request);

	request[len++] = '\n';

	if(c == mesh->everyone) {
		broadcast_meta(mesh, NULL, request, len);
		return 0;
	} else
		return send_meta(mesh, c, request, len);
}

void forward_request(meshlink_handle_t *mesh, connection_t *from, const char *request) {
	logger(mesh, MESHLINK_DEBUG, "Forwarding %s from %s (%s): %s", request_name[atoi(request)], from->name, from->hostname, request);

	// Create a temporary newline-terminated copy of the request
	int len = strlen(request);
	char tmp[len + 1];
	memcpy(tmp, request, len);
	tmp[len] = '\n';
	broadcast_meta(mesh, from, tmp, sizeof tmp);
}

bool receive_request(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	if(c->outgoing && mesh->proxytype == PROXY_HTTP && c->allow_request == ID) {
		if(!request[0] || request[0] == '\r')
			return true;
		if(!strncasecmp(request, "HTTP/1.1 ", 9)) {
			if(!strncmp(request + 9, "200", 3)) {
				logger(mesh, MESHLINK_DEBUG, "Proxy request granted");
				return true;
			} else {
				logger(mesh, MESHLINK_DEBUG, "Proxy request rejected: %s", request + 9);
				return false;
			}
		}
	}

	int reqno = atoi(request);

	if(reqno || *request == '0') {
		if((reqno < 0) || (reqno >= LAST) || !request_handlers[reqno]) {
			logger(mesh, MESHLINK_DEBUG, "Unknown request from %s (%s): %s", c->name, c->hostname, request);
			return false;
		} else {
			logger(mesh, MESHLINK_DEBUG, "Got %s from %s (%s): %s", request_name[reqno], c->name, c->hostname, request);
		}

		if((c->allow_request != ALL) && (c->allow_request != reqno)) {
			logger(mesh, MESHLINK_ERROR, "Unauthorized request from %s (%s)", c->name, c->hostname);
			return false;
		}

		if(!request_handlers[reqno](mesh, c, request)) {
			/* Something went wrong. Probably scriptkiddies. Terminate. */

			logger(mesh, MESHLINK_ERROR, "Error while processing %s from %s (%s)", request_name[reqno], c->name, c->hostname);
			return false;
		}
	} else {
		logger(mesh, MESHLINK_ERROR, "Bogus data received from %s (%s)", c->name, c->hostname);
		return false;
	}

	return true;
}

static int past_request_compare(const past_request_t *a, const past_request_t *b) {
	return strcmp(a->request, b->request);
}

static void free_past_request(past_request_t *r) {
	if(r->request)
		free((void *)r->request);

	free(r);
}

static void age_past_requests(event_loop_t *loop, void *data) {
	meshlink_handle_t *mesh = loop->data;
	int left = 0, deleted = 0;

	for splay_each(past_request_t, p, mesh->past_request_tree) {
		if(p->firstseen + mesh->pinginterval <= mesh->loop.now.tv_sec)
			splay_delete_node(mesh->past_request_tree, node), deleted++;
		else
			left++;
	}

	if(left || deleted)
		logger(mesh, MESHLINK_DEBUG, "Aging past requests: deleted %d, left %d", deleted, left);

	if(left)
		timeout_set(&mesh->loop, &mesh->past_request_timeout, &(struct timeval){10, rand() % 100000});
}

bool seen_request(meshlink_handle_t *mesh, const char *request) {
	past_request_t *new, p = {NULL};

	p.request = request;

	if(splay_search(mesh->past_request_tree, &p)) {
		logger(mesh, MESHLINK_DEBUG, "Already seen request");
		return true;
	} else {
		new = xmalloc(sizeof *new);
		new->request = xstrdup(request);
		new->firstseen = mesh->loop.now.tv_sec;
		splay_insert(mesh->past_request_tree, new);
		timeout_add(&mesh->loop, &mesh->past_request_timeout, age_past_requests, NULL, &(struct timeval){10, rand() % 100000});
		return false;
	}
}

void init_requests(meshlink_handle_t *mesh) {
	mesh->past_request_tree = splay_alloc_tree((splay_compare_t) past_request_compare, (splay_action_t) free_past_request);
}

void exit_requests(meshlink_handle_t *mesh) {
	if(mesh->past_request_tree)
		splay_delete_tree(mesh->past_request_tree);
	mesh->past_request_tree = NULL;

	timeout_del(&mesh->loop, &mesh->past_request_timeout);
}
