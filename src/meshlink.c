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
#include "protocol.h"
#include "xalloc.h"

static const char *errstr[] = {
	[MESHLINK_OK] = "No error",
	[MESHLINK_ENOMEM] = "Out of memory",
	[MESHLINK_ENOENT] = "No such node",
};

const char *meshlink_strerror(meshlink_errno_t errno) {
	return errstr[errno];
}

static meshlink_handle_t *meshlink_setup(meshlink_handle_t *mesh) {
	return mesh;
}

meshlink_handle_t *meshlink_open(const char *confbase, const char *name) {
	if(!confbase || !*confbase) {
		fprintf(stderr, "No confbase given!\n");
		return NULL;
	}

	if(!name || !*name) {
		fprintf(stderr, "No name given!\n");
		return NULL;
	}

	if(!check_id(name)) {
		fprintf(stderr, "Invalid name given!\n");
		return NULL;
	}

	meshlink_handle_t *mesh = xzalloc(sizeof *mesh);
	mesh->confbase = xstrdup(confbase);
	mesh->name = xstrdup(name);

	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", confbase);

	FILE *f = fopen(filename, "r");

	if(!f && errno == ENOENT)
		return meshlink_setup(mesh);

	if(!f) {
		fprintf(stderr, "Could not open %s: %s\n", filename, strerror(errno));
		return meshlink_close(mesh), NULL;
	}

	char buf[1024] = "";
	if(!fgets(buf, sizeof buf, f)) {
		fprintf(stderr, "Could not read line from %s: %s\n", filename, strerror(errno));
		fclose(f);
		return meshlink_close(mesh), NULL;
	}

	fclose(f);

	size_t len = strlen(buf);
	if(len && buf[len - 1] == '\n')
		buf[--len] = 0;
	if(len && buf[len - 1] == '\r')
		buf[--len] = 0;

	if(strncmp(buf, "Name = ", 7) || !check_id(buf + 7)) {
		fprintf(stderr, "Could not read Name from %s\n", filename);
		return meshlink_close(mesh), NULL;
	}

	if(strcmp(buf + 7, name)) {
		fprintf(stderr, "Name in %s is %s, not the same as %s\n", filename, buf + 7, name);
		free(mesh->name);
		mesh->name = xstrdup(buf + 7);
	}

	snprintf(filename, sizeof filename, "%s" SLASH "ed25519_key.priv", mesh->confbase);
	f = fopen(filename, "r");
	if(!f) {
		fprintf(stderr, "Could not open %s: %s\n", filename, strerror(errno));
		return meshlink_close(mesh), NULL;
	}

	mesh->self->ecdsa = ecdsa_read_pem_private_key(f);
	fclose(f);

	if(!mesh->self->ecdsa) {
		fprintf(stderr, "Could not read keypair!\n");
		return meshlink_close(mesh), NULL;
	}

	return mesh;
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

