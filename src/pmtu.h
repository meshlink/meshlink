#ifndef MESHLINK_PMTU_H
#define MESHLINK_PMTU_H

/*
    pmtu.h -- header for pmtu.c
    Copyright (C) 2020 Guus Sliepen <guus@meshlink.io>

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

#define MIN_PROBE_SIZE 4

extern void keepalive(struct meshlink_handle *mesh, struct node_t *n, bool traffic);
extern void udp_probe_h(struct meshlink_handle *mesh, struct node_t *n, struct vpn_packet_t *packet, uint16_t len);

#endif
