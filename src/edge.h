/*
    edge.h -- header for edge.c
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

#ifndef __MESHLINK_EDGE_H__
#define __MESHLINK_EDGE_H__

#include "splay_tree.h"
#include "connection.h"
#include "net.h"
#include "node.h"

typedef struct edge_t {
	struct node_t *from;
	struct node_t *to;
	sockaddr_t address;

	uint32_t options;                       /* options turned on for this edge */
	int weight;                             /* weight of this edge */

	struct connection_t *connection;        /* connection associated with this edge, if available */
	struct edge_t *reverse;                 /* edge in the opposite direction, if available */
} edge_t;

extern void init_edges(struct meshlink_handle *mesh);
extern void exit_edges(struct meshlink_handle *mesh);
extern edge_t *new_edge(void) __attribute__ ((__malloc__));
extern void free_edge(edge_t *);
extern struct splay_tree_t *new_edge_tree(void) __attribute__ ((__malloc__));
extern void free_edge_tree(struct splay_tree_t *);
extern void edge_add(struct meshlink_handle *mesh, edge_t *);
extern void edge_del(struct meshlink_handle *mesh, edge_t *);
extern edge_t *lookup_edge(struct node_t *, struct node_t *);

#endif /* __MESHLINK_EDGE_H__ */
