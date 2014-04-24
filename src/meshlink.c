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
#include <pthread.h>

#include "meshlink_internal.h"
#include "node.h"
#include "protocol.h"
#include "route.h"
#include "xalloc.h"

meshlink_handle_t *mesh;

static const char *errstr[] = {
	[MESHLINK_OK] = "No error",
	[MESHLINK_ENOMEM] = "Out of memory",
	[MESHLINK_ENOENT] = "No such node",
};

const char *meshlink_strerror(meshlink_errno_t errno) {
	return errstr[errno];
}

// TODO: hack, remove once all global variables are gone.
static void set_mesh(meshlink_handle_t *localmesh) {
	mesh = localmesh;
}

static meshlink_handle_t *meshlink_setup(meshlink_handle_t *mesh) {
	set_mesh(mesh);
	return mesh;
}

meshlink_handle_t *meshlink_open(const char *confbase, const char *name) {
	// Validate arguments provided by the application

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

	// Check whether meshlink.conf already exists

	char filename[PATH_MAX];
	snprintf(filename, sizeof filename, "%s" SLASH "meshlink.conf", confbase);

	if(access(filename, R_OK)) {
		if(errno == ENOENT) {
			// If not, create it
			meshlink_setup(mesh);
		} else {
			fprintf(stderr, "Cannot not read from %s: %s\n", filename, strerror(errno));
			return meshlink_close(mesh), NULL;
		}
	}

	// Read the configuration

	init_configuration(&mesh->config);

	if(!read_server_config())
		return meshlink_close(mesh), NULL;

	// Setup up everything
	// TODO: we should not open listening sockets yet

	if(!setup_network())
		return meshlink_close(mesh), NULL;

	set_mesh(mesh);
	return mesh;
}

void *meshlink_main_loop(void *arg) {
	meshlink_handle_t *mesh = arg;

	try_outgoing_connections();

	main_loop();

	return NULL;
}

bool meshlink_start(meshlink_handle_t *mesh) {
	// TODO: open listening sockets first

	// Start the main thread

	if(pthread_create(&mesh->thread, NULL, meshlink_main_loop, mesh) != 0) {
		fprintf(stderr, "Could not start thread: %s\n", strerror(errno));
		memset(&mesh->thread, 0, sizeof mesh->thread);
		return false;
	}

	return true;
}

void meshlink_stop(meshlink_handle_t *mesh) {
	// TODO: close the listening sockets to signal the main thread to shut down

	// Wait for the main thread to finish

	pthread_join(mesh->thread, NULL);
}

void meshlink_close(meshlink_handle_t *mesh) {
	// Close and free all resources used.

	close_network_connections();

	logger(DEBUG_ALWAYS, LOG_NOTICE, "Terminating");

	exit_configuration(&mesh->config);

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
	vpn_packet_t packet;
	meshlink_packethdr_t *hdr = (meshlink_packethdr_t *)packet.data;
	if (sizeof(meshlink_packethdr_t) + len > MAXSIZE) {
		//log something
		return false;
	}

	packet.probe = false;
	memset(hdr, 0, sizeof *hdr);
	memcpy(hdr->destination, destination->name, sizeof hdr->destination);
	memcpy(hdr->source, mesh->self->name, sizeof hdr->source);

	packet.len = sizeof *hdr + len;
	memcpy(packet.data + sizeof *hdr, data, len);

        mesh->self->in_packets++;
        mesh->self->in_bytes += packet.len;
        route(mesh->self, &packet);
	return false;
}

meshlink_node_t *meshlink_get_node(meshlink_handle_t *mesh, const char *name) {
	return (meshlink_node_t *)lookup_node(name);
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

