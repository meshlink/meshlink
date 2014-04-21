/*
    meshlink.c -- Implementation of the MeshLink API.
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

#include "meshlink_internal.h"

static const char *errstr[] = {
	[MESHLINK_OK] = "No error",
	[MESHLINK_ENOMEM] = "Out of memory",
	[MESHLINK_ENOENT] = "No such node",
};

const char *meshlink_strerror(meshlink_errno_t errno) {
	return errstr[errno];
}

meshlink_handle_t *meshlink_open(const char *confbase, const char *name) {
	return NULL;
}

bool meshlink_start(meshlink_handle_t *mesh) {
	return false;
}

void meshlink_stop(meshlink_handle_t *mesh) {
}

void meshlink_close(meshlink_handle_t *mesh) {
}

void meshlink_set_receive_cb(meshlink_handle_t *mesh, meshlink_receive_cb_t cb) {
	mesh->receive_cb = cb;
}

void meshlink_set_node_status_cb(meshlink_handle_t *mesh, meshlink_node_status_cb_t cb) {
	mesh->node_status_cb = cb;
}

void meshlink_set_log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, meshlink_log_cb_t cb) {
	mesh->log_cb = cb;
	mesh->log_level = level;
}

bool meshlink_send(meshlink_handle_t *mesh, meshlink_node_t *destination, const void *data, unsigned int len) {
	return false;
}

meshlink_node_t *meshlink_get_node(meshlink_handle_t *mesh, const char *name) {
	return NULL;
}

size_t meshlink_get_all_nodes(meshlink_handle_t *mesh, meshlink_node_t **nodes, size_t nmemb) {
	return 0;
}

char *meshlink_sign(meshlink_handle_t *mesh, const char *data, size_t len) {
	return NULL;
}

bool meshlink_verify(meshlink_handle_t *mesh, meshlink_node_t *source, const char *data, size_t len, const char *signature) {
	return false;
}

char *meshlink_invite(meshlink_handle_t *mesh, const char *name) {
	return NULL;
}

bool meshlink_join(meshlink_handle_t *mesh, const char *invitation) {
	return false;
}

char *meshlink_export(meshlink_handle_t *mesh) {
	return NULL;
}

bool meshlink_import(meshlink_handle_t *mesh, const char *data) {
	return false;
}

void meshlink_blacklist(meshlink_handle_t *mesh, meshlink_node_t *node) {
}

