/*
    protocol_misc.c -- handle the meta-protocol, miscellaneous functions
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
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "utils.h"

int maxoutbufsize = 0;

/* Status and error notification routines */

bool send_status(meshlink_handle_t *mesh, connection_t *c, int statusno, const char *statusstring) {
	if(!statusstring)
		statusstring = "Status";

	int err = send_request(mesh, c, "%d %d %s", STATUS, statusno, statusstring);
    if(err) {
        logger(mesh, MESHLINK_ERROR, "send_status() for connection %p failed with err=%d.\n", c, err);
    }
	return !err;
}

bool status_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	int statusno;
	char statusstring[MAX_STRING_SIZE];

	if(sscanf(request, "%*d %d " MAX_STRING, &statusno, statusstring) != 2) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s)", "STATUS",
			   c->name, c->hostname);
		return false;
	}

	logger(mesh, MESHLINK_INFO, "Status message from %s (%s): %d: %s",
			   c->name, c->hostname, statusno, statusstring);

	return true;
}

bool send_error(meshlink_handle_t *mesh, connection_t *c, int err, const char *errstring) {
	if(!errstring)
		errstring = "Error";

	int err = send_request(mesh, c, "%d %d %s", ERROR, err, errstring);
    if(err) {
        logger(mesh, MESHLINK_ERROR, "send_error() for connection %p failed with err=%d.\n", c, err);
    }
	return !err;
}

bool error_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	int err;
	char errorstring[MAX_STRING_SIZE];

	if(sscanf(request, "%*d %d " MAX_STRING, &err, errorstring) != 2) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s)", "ERROR",
			   c->name, c->hostname);
		return false;
	}

	logger(mesh, MESHLINK_INFO, "Error message from %s (%s): %d: %s",
			   c->name, c->hostname, err, errorstring);

	return false;
}

bool send_termreq(meshlink_handle_t *mesh, connection_t *c) {
	int err = send_request(mesh, c, "%d", TERMREQ);
    if(err) {
        logger(mesh, MESHLINK_ERROR, "send_termreq() for connection %p failed with err=%d.\n", c, err);
    }
	return !err;
}

bool termreq_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	return false;
}

bool send_ping(meshlink_handle_t *mesh, connection_t *c) {
	c->status.pinged = true;
	c->last_ping_time = mesh->loop.now.tv_sec;

	int err = send_request(mesh, c, "%d", PING);
    if(err) {
        logger(mesh, MESHLINK_ERROR, "send_ping() for connection %p failed with err=%d.\n", c, err);
    }
	return !err;
}

bool ping_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	return send_pong(mesh, c);
}

bool send_pong(meshlink_handle_t *mesh, connection_t *c) {
	int err = send_request(mesh, c, "%d", PONG);
    if(err) {
        logger(mesh, MESHLINK_ERROR, "send_pong() for connection %p failed with err=%d.\n", c, err);
    }
	return !err;
}

bool pong_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	c->status.pinged = false;

	/* Succesful connection, reset timeout if this is an outgoing connection. */

	if(c->outgoing) {
		c->outgoing->timeout = 0;
		c->outgoing->cfg = NULL;
		if(c->outgoing->ai)
			freeaddrinfo(c->outgoing->ai);
		c->outgoing->ai = NULL;
		c->outgoing->aip = NULL;
	}

	return true;
}

/* Sending and receiving packets via TCP */

bool send_tcppacket(meshlink_handle_t *mesh, connection_t *c, const vpn_packet_t *packet) {
	/* If there already is a lot of data in the outbuf buffer, discard this packet.
	   We use a very simple Random Early Drop algorithm. */

	if(2.0 * c->outbuf.len / (float)maxoutbufsize - 1 > (float)rand()/(float)RAND_MAX)
		return true;

	if(0 != send_request(mesh, c, "%d %hd", PACKET, packet->len))
		return false;

	return !send_meta(mesh, c, (char *)packet->data, packet->len);
}

bool tcppacket_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	short int len;

	if(sscanf(request, "%*d %hd", &len) != 1) {
		logger(mesh, MESHLINK_ERROR, "Got bad %s from %s (%s)", "PACKET", c->name,
			   c->hostname);
		return false;
	}

	/* Set reqlen to len, this will tell receive_meta() that a tcppacket is coming. */

	c->tcplen = len;

	return true;
}
