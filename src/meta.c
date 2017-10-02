/*
    meta.c -- handle the meta communication
    Copyright (C) 2014-2017 Guus Sliepen <guus@meshlink.io>,

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
#include "meta.h"
#include "net.h"
#include "protocol.h"
#include "utils.h"
#include "xalloc.h"

bool send_meta_sptps(void *handle, uint8_t type, const void *buffer, size_t length) {
	(void)type;
	connection_t *c = handle;
	meshlink_handle_t *mesh = c->mesh;

	if(!c) {
		logger(mesh, MESHLINK_ERROR, "send_meta_sptps() called with NULL pointer!");
		abort();
	}

	buffer_add(&c->outbuf, (const char *)buffer, length);
	io_set(&mesh->loop, &c->io, IO_READ | IO_WRITE);

	return true;
}

bool send_meta(meshlink_handle_t *mesh, connection_t *c, const char *buffer, int length) {
	if(!c) {
		logger(mesh, MESHLINK_ERROR, "send_meta() called with NULL pointer!");
		abort();
	}

	logger(mesh, MESHLINK_DEBUG, "Sending %d bytes of metadata to %s", length, c->name);

	if(c->allow_request == ID) {
		buffer_add(&c->outbuf, buffer, length);
		io_set(&mesh->loop, &c->io, IO_READ | IO_WRITE);
		return true;
	}

	return sptps_send_record(&c->sptps, 0, buffer, length);
}

void broadcast_meta(meshlink_handle_t *mesh, connection_t *from, const char *buffer, int length) {
	for list_each(connection_t, c, mesh->connections)
		if(c != from && c->status.active)
			send_meta(mesh, c, buffer, length);
}

bool receive_meta_sptps(void *handle, uint8_t type, const void *data, uint16_t length) {
	connection_t *c = handle;
	meshlink_handle_t *mesh = c->mesh;
	char *request = (char *)data;

	if(!c) {
		logger(mesh, MESHLINK_ERROR, "receive_meta_sptps() called with NULL pointer!");
		abort();
	}

	if(type == SPTPS_HANDSHAKE) {
		if(c->allow_request == ACK)
			return send_ack(mesh, c);
		else
			return true;
	}

	if(!request)
		return true;

	/* Are we receiving a TCPpacket? */

	if(c->tcplen) {
		abort(); // TODO: get rid of tcplen altogether
	}

	/* Change newline to null byte, just like non-SPTPS requests */

	if(request[length - 1] == '\n')
		request[length - 1] = 0;

	/* Otherwise we are waiting for a request */

	return receive_request(mesh, c, request);
}

bool receive_meta(meshlink_handle_t *mesh, connection_t *c) {
	int inlen;
	char inbuf[MAXBUFSIZE];

	inlen = recv(c->socket, inbuf, sizeof(inbuf), 0);

	if(inlen <= 0) {
		if(!inlen || !errno) {
			logger(mesh, MESHLINK_INFO, "Connection closed by %s", c->name);
		} else if(sockwouldblock(sockerrno))
			return true;
		else
			logger(mesh, MESHLINK_ERROR, "Metadata socket read error for %s: %s", c->name, sockstrerror(sockerrno));
		return false;
	}

	if(c->allow_request == ID) {
		buffer_add(&c->inbuf, inbuf, inlen);

		char *request = buffer_readline(&c->inbuf);

		if(request) {
			if(!receive_request(mesh, c, request) || c->allow_request == ID)
				return false;

			int left = c->inbuf.len - c->inbuf.offset;
			if(left > 0) {
				fprintf(stderr, "GOT A LITTLE MORE\n");
				return sptps_receive_data(&c->sptps, buffer_read(&c->inbuf, left), left);
			} else
				return true;
		}

		if(c->inbuf.len >= sizeof(inbuf)) {
			logger(mesh, MESHLINK_ERROR, "Input buffer full for %s", c->name);
			return false;
		} else
			return true;
	}

	return sptps_receive_data(&c->sptps, inbuf, inlen);
}
