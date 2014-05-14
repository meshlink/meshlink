/*
    meta.c -- handle the meta communication
    Copyright (C) 2014 Guus Sliepen <guus@meshlink.io>,

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
	connection_t *c = handle;
	meshlink_handle_t *mesh = c->mesh;

	if(!c) {
		logger(DEBUG_ALWAYS, LOG_ERR, "send_meta_sptps() called with NULL pointer!");
		abort();
	}

	buffer_add(&c->outbuf, (const char *)buffer, length);
	io_set(&mesh->loop, &c->io, IO_READ | IO_WRITE);

	return true;
}

bool send_meta(meshlink_handle_t *mesh, connection_t *c, const char *buffer, int length) {
	if(!c) {
		logger(DEBUG_ALWAYS, LOG_ERR, "send_meta() called with NULL pointer!");
		abort();
	}

	logger(DEBUG_META, LOG_DEBUG, "Sending %d bytes of metadata to %s (%s)", length,
			   c->name, c->hostname);

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
		logger(DEBUG_ALWAYS, LOG_ERR, "receive_meta_sptps() called with NULL pointer!");
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
		if(length != c->tcplen)
			return false;
		receive_tcppacket(mesh, c, request, length);
		c->tcplen = 0;
		return true;
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
	char *bufp = inbuf, *endp;

	/* Strategy:
	   - Read as much as possible from the TCP socket in one go.
	   - Decrypt it.
	   - Check if a full request is in the input buffer.
	   - If yes, process request and remove it from the buffer,
	   then check again.
	   - If not, keep stuff in buffer and exit.
	 */

	buffer_compact(&c->inbuf, MAXBUFSIZE);

	if(sizeof inbuf <= c->inbuf.len) {
		logger(DEBUG_ALWAYS, LOG_ERR, "Input buffer full for %s (%s)", c->name, c->hostname);
		return false;
	}

	inlen = recv(c->socket, inbuf, sizeof inbuf - c->inbuf.len, 0);

	if(inlen <= 0) {
		if(!inlen || !errno) {
			logger(DEBUG_CONNECTIONS, LOG_NOTICE, "Connection closed by %s (%s)",
					   c->name, c->hostname);
		} else if(sockwouldblock(sockerrno))
			return true;
		else
			logger(DEBUG_ALWAYS, LOG_ERR, "Metadata socket read error for %s (%s): %s",
				   c->name, c->hostname, sockstrerror(sockerrno));
		return false;
	}

	if(c->allow_request == ID) {
		endp = memchr(bufp, '\n', inlen);
		if(endp)
			endp++;
		else
			endp = bufp + inlen;

		buffer_add(&c->inbuf, bufp, endp - bufp);

		inlen -= endp - bufp;
		bufp = endp;

		while(c->inbuf.len) {
			char *request = buffer_readline(&c->inbuf);
			if(request) {
				bool result = receive_request(mesh, c, request);
				if(!result)
					return false;
				continue;
			} else {
				break;
			}
		}

		return true;
	}

	return sptps_receive_data(&c->sptps, bufp, inlen);
}
