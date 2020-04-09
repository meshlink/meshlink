#ifndef MESHLINK_ADNS_H
#define MESHLINK_ADNS_H

/*
    adns.h -- header file for adns.c
    Copyright (C) 2019 Guus Sliepen <guus@meshlink.io>

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

#include "meshlink_internal.h"

typedef void (*adns_cb_t)(meshlink_handle_t *mesh, char *host, char *serv, void *data, struct addrinfo *ai, int err);

extern void init_adns(meshlink_handle_t *mesh);
extern void exit_adns(meshlink_handle_t *mesh);
extern void adns_queue(meshlink_handle_t *mesh, char *host, char *serv, adns_cb_t cb, void *data, int timeout);
extern struct addrinfo *adns_blocking_request(meshlink_handle_t *mesh, char *host, char *serv, int timeout);

#endif
