/*
    protocol_misc.c -- handle the meta-protocol, miscellaneous functions
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
#include "logger.h"
#include "meshlink_internal.h"
#include "meta.h"
#include "net.h"
#include "netutl.h"
#include "protocol.h"
#include "utils.h"

bool send_ping(meshlink_handle_t *mesh, connection_t *c) {
	c->status.pinged = true;
	c->last_ping_time = mesh->loop.now.tv_sec;

	return send_request(mesh, c, "%d", PING);
}

bool ping_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	(void)request;
	return send_pong(mesh, c);
}

bool send_pong(meshlink_handle_t *mesh, connection_t *c) {
	return send_request(mesh, c, "%d", PONG);
}

bool pong_h(meshlink_handle_t *mesh, connection_t *c, const char *request) {
	(void)mesh;
	(void)request;
	c->status.pinged = false;

	/* Succesful connection, reset timeout if this is an outgoing connection. */

	if(c->outgoing) {
		c->outgoing->timeout = 0;
		c->outgoing->cfg = NULL;

		if(c->outgoing->ai) {
			freeaddrinfo(c->outgoing->ai);
		}

		c->outgoing->ai = NULL;
		c->outgoing->aip = NULL;
	}

	return true;
}
