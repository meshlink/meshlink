#ifndef MESHLINK_SUBMESH_H
#define MESHLINK_SUBMESH_H

/*
    submesh.h -- header for submesh.c
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

#define CORE_MESH "."

typedef struct submesh_t {
	char *name;                             /* name of this Sub-Mesh */
	void *priv;

	struct meshlink_handle *mesh;                   /* the mesh this submesh belongs to */
} submesh_t;

extern void init_submeshes(struct meshlink_handle *mesh);
extern void exit_submeshes(struct meshlink_handle *mesh);
extern submesh_t *new_submesh(void) __attribute__((__malloc__));
extern void free_submesh(submesh_t *);
extern submesh_t *create_submesh(struct meshlink_handle *mesh, const char *);
extern void submesh_add(struct meshlink_handle *mesh, submesh_t *);
extern void submesh_del(struct meshlink_handle *mesh, submesh_t *);
extern submesh_t *lookup_submesh(struct meshlink_handle *mesh, const char *);
extern submesh_t *lookup_or_create_submesh(struct meshlink_handle *mesh, const char *);
extern bool submesh_allows_node(const submesh_t *submesh, const struct node_t *node);

#endif
