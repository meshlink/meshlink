/*
    node.c -- node tree management
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

#include "hash.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "net.h"
#include "netutl.h"
#include "submesh.h"
#include "splay_tree.h"
#include "utils.h"
#include "xalloc.h"

void init_submeshes(meshlink_handle_t *mesh) {
	mesh->submeshes = list_alloc((list_action_t)free_submesh);
}

void exit_submeshes(meshlink_handle_t *mesh) {
	list_delete_list(mesh->submeshes);
	mesh->submeshes = NULL;
}

submesh_t *new_submesh(void) {
	submesh_t *s = xzalloc(sizeof(*s));

	s->name = NULL;
	s->priv = NULL;

	return s;
}

void free_submesh(submesh_t *s) {
	if(s->name) {
		free(s->name);
	}

	free(s);
}

void submesh_add(meshlink_handle_t *mesh, submesh_t *s) {
	s->mesh = mesh;
	list_insert_tail(mesh->submeshes, (void *)s);
}

void submesh_del(meshlink_handle_t *mesh, submesh_t *s) {
	list_delete(mesh->submeshes, (void *)s);
}

submesh_t *lookup_submesh(struct meshlink_handle *mesh, const char *submesh_name) {
	submesh_t *submesh = NULL;

	if(!mesh->submeshes) {
		return NULL;
	}

	for list_each(submesh_t, s, mesh->submeshes) {
		if(!strcmp(submesh_name, s->name)) {
			submesh = s;
			break;
		}
	}

	return submesh;
}