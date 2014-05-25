/*
    route.h -- header file for route.c
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

#ifndef __MESHLINK_ROUTE_H__
#define __MESHLINK_ROUTE_H__

#include "net.h"
#include "node.h"

extern bool decrement_ttl;

extern void route(struct meshlink_handle *mesh, struct node_t *, struct vpn_packet_t *);

#endif /* __MESHLINK_ROUTE_H__ */
