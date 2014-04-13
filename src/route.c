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
#include "net.h"
#include "route.h"
#include "utils.h"
#include "libmeshlink.h"

rmode_t routing_mode = RMODE_ROUTER;
fmode_t forwarding_mode = FMODE_INTERNAL;
bmode_t broadcast_mode = BMODE_MST;
bool decrement_ttl = false;
bool directonly = false;
bool priorityinheritance = false;
int macexpire = 600;
mac_t mymac = {{0xFE, 0xFD, 0, 0, 0, 0}};
bool pcap = false;

static bool ratelimit(int frequency) {
	static time_t lasttime = 0;
	static int count = 0;

	if(lasttime == now.tv_sec) {
		if(count >= frequency)
			return true;
	} else {
		lasttime = now.tv_sec;
		count = 0;
	}

	count++;
	return false;
}

static bool checklength(node_t *source, vpn_packet_t *packet, length_t length) {
	if(packet->len < length) {
		logger(DEBUG_TRAFFIC, LOG_WARNING, "Got too short packet from %s (%s)", source->name, source->hostname);
		return false;
	} else
		return true;
}

void route(node_t *source,vpn_packet_t *packet) {
    // TODO: route on name or key

    node_t* owner = NULL;
    tincpackethdr* hdr = (tincpackethdr*)packet->data;
    owner = lookup_node(hdr->destination);
    if (owner == NULL) {
    //Lookup failed
    logger(DEBUG_TRAFFIC, LOG_WARNING, "Cant lookup the owner of a packet in the route() function. This should never happen \n");
    return;
    }

    if (owner == myself ) {
    //TODO: implement sending received data from meshlink library to the application
    logger(DEBUG_TRAFFIC, LOG_WARNING, "I received a packet for me with payload: %s \n", packet->data + 46);
    (recv_callback)(packet->data + 46);
    return;
    }

    if(!owner->status.reachable) {
    //TODO: check what to do here, not just print a warning
    logger(DEBUG_TRAFFIC, LOG_WARNING, "The owner of a packet in the route() function is unreachable. Dropping packet. \n");
    return;
    }

    //TODO: I skipped here a lot of checks !

    send_packet(owner,packet);
    return;
}
