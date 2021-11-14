/*
    route.c -- routing
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

#include "logger.h"
#include "meshlink_internal.h"
#include "net.h"
#include "route.h"
#include "utils.h"

static bool checklength(node_t *source, vpn_packet_t *packet, uint16_t length) {
	assert(length);

	if(packet->len < length) {
		logger(source->mesh, MESHLINK_WARNING, "Got too short packet from %s", source->name);
		return false;
	} else {
		return true;
	}
}

void route(meshlink_handle_t *mesh, node_t *source, vpn_packet_t *packet) {
	assert(source);

	// TODO: route on name or key

	meshlink_packethdr_t *hdr = (meshlink_packethdr_t *) packet->data;
	node_t *dest = lookup_node(mesh, (char *)hdr->destination);
	logger(mesh, MESHLINK_DEBUG, "Routing packet from \"%s\" to \"%s\"\n", hdr->source, hdr->destination);

	//Check Length
	if(!checklength(source, packet, sizeof(*hdr))) {
		return;
	}

	if(dest == NULL) {
		//Lookup failed
		logger(mesh, MESHLINK_WARNING, "Can't lookup the destination of a packet in the route() function. This should never happen!\n");
		logger(mesh, MESHLINK_WARNING, "Destination was: %s\n", hdr->destination);
		return;
	}

	size_t len = packet->len - sizeof(*hdr);

	// Channel traffic accounting
	if(source == mesh->self) {
		dest->out_data += len + SPTPS_OVERHEAD;
		mesh->self->out_data += len + SPTPS_OVERHEAD;
	}

	if(dest == mesh->self) {
		source->in_data += len + SPTPS_OVERHEAD;
		mesh->self->in_data += len + SPTPS_OVERHEAD;
		const void *payload = packet->data + sizeof(*hdr);

		char hex[len * 2 + 1];

		if(mesh->log_level <= MESHLINK_DEBUG) {
			bin2hex(payload, hex, len);        // don't do this unless it's going to be logged
		}

		logger(mesh, MESHLINK_DEBUG, "I received a packet for me with payload: %s\n", hex);

		if(source->utcp) {
			channel_receive(mesh, (meshlink_node_t *)source, payload, len);
		} else if(mesh->receive_cb) {
			mesh->receive_cb(mesh, (meshlink_node_t *)source, payload, len);
		}

		return;
	}

	if(!dest->status.reachable) {
		//TODO: check what to do here, not just print a warning
		logger(mesh, MESHLINK_WARNING, "The destination of a packet in the route() function is unreachable. Dropping packet.\n");
		return;
	}

	if(dest == source) {
		logger(mesh, MESHLINK_ERROR, "Routing loop for packet from %s!", source->name);
		return;
	}

	send_packet(mesh, dest, packet);
	return;
}
