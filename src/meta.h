#ifndef MESHLINK_META_H
#define MESHLINK_META_H

/*
    meta.h -- header for meta.c
    Copyright (C) 2014, 2017 Guus Sliepen <guus@meshlink.io>

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

#include "connection.h"

extern bool send_meta(struct meshlink_handle *mesh, struct connection_t *, const char *, int);
extern bool send_meta_sptps(void *, uint8_t, const void *, size_t);
extern bool receive_meta_sptps(void *, uint8_t, const void *, uint16_t);
extern void broadcast_meta(struct meshlink_handle *mesh, struct connection_t *, const char *, int);
extern void broadcast_submesh_meta(struct meshlink_handle *mesh, connection_t *from, submesh_t *s,
                                   const char *buffer, int length);
extern bool receive_meta(struct meshlink_handle *mesh, struct connection_t *) __attribute__((__warn_unused_result__));

#endif
