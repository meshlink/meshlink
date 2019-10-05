/*
    submesh.c -- submesh management
    Copyright (C) 2019 Guus Sliepen <guus@meshlink.io>,

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
#include "protocol.h"

static submesh_t *new_submesh(void) {
	return xzalloc(sizeof(submesh_t));
}

static void free_submesh(submesh_t *s) {
	free(s->name);
	free(s);
}

void init_submeshes(meshlink_handle_t *mesh) {
	assert(!mesh->submeshes);
	mesh->submeshes = list_alloc((list_action_t)free_submesh);
}

void exit_submeshes(meshlink_handle_t *mesh) {
	if(mesh->submeshes) {
		list_delete_list(mesh->submeshes);
	}

	mesh->submeshes = NULL;
}

static submesh_t *submesh_add(meshlink_handle_t *mesh, const char *submesh) {
	assert(submesh);

	submesh_t *s = new_submesh();
	s->name = xstrdup(submesh);
	list_insert_tail(mesh->submeshes, (void *)s);
	return s;
}

submesh_t *create_submesh(meshlink_handle_t *mesh, const char *submesh) {
	assert(submesh);

	if(0 == strcmp(submesh, CORE_MESH)) {
		logger(NULL, MESHLINK_ERROR, "Cannot create submesh handle for core mesh!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!check_id(submesh)) {
		logger(NULL, MESHLINK_ERROR, "Invalid SubMesh Id!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(lookup_submesh(mesh, submesh)) {
		logger(NULL, MESHLINK_ERROR, "SubMesh Already exists!\n");
		meshlink_errno = MESHLINK_EEXIST;
		return NULL;
	}

	return submesh_add(mesh, submesh);
}

submesh_t *lookup_or_create_submesh(meshlink_handle_t *mesh, const char *submesh) {
	assert(submesh);

	if(0 == strcmp(submesh, CORE_MESH)) {
		logger(NULL, MESHLINK_ERROR, "Cannot create submesh handle for core mesh!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	if(!check_id(submesh)) {
		logger(NULL, MESHLINK_ERROR, "Invalid SubMesh Id!\n");
		meshlink_errno = MESHLINK_EINVAL;
		return NULL;
	}

	submesh_t *s = lookup_submesh(mesh, submesh);

	if(s) {
		meshlink_errno = MESHLINK_OK;
		return s;
	}

	return submesh_add(mesh, submesh);
}

submesh_t *lookup_submesh(struct meshlink_handle *mesh, const char *submesh_name) {
	assert(submesh_name);

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

bool submesh_allows_node(const submesh_t *submesh, const node_t *node) {
	if(!node->submesh || !submesh || submesh == node->submesh) {
		return true;
	} else {
		return false;
	}
}
