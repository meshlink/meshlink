#ifndef MESHLINK_GRAPH_H
#define MESHLINK_GRAPH_H

/*
    graph.h -- header for graph.c
    Copyright (C) 2014-2019 Guus Sliepen <guus@meshlink.io>

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

extern void graph_add_edge(struct meshlink_handle *mesh, edge_t *e);
extern void graph_del_edge(struct meshlink_handle *mesh, edge_t *e);

#endif
