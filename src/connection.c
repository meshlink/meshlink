/*
    connection.c -- connection list management
    Copyright (C) 2000-2013 Guus Sliepen <guus@meshlink.io>

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

#include "list.h"
#include "conf.h"
#include "connection.h"
#include "list.h"
#include "logger.h"
#include "meshlink_internal.h"
#include "utils.h"
#include "xalloc.h"

void init_connections(meshlink_handle_t *mesh) {
	mesh->connections = list_alloc((list_action_t) free_connection);
	mesh->everyone = new_connection();
	mesh->everyone->name = xstrdup("mesh->everyone");
}

void exit_connections(meshlink_handle_t *mesh) {
	if(mesh->connections) {
		list_delete_list(mesh->connections);
	}

	free_connection(mesh->everyone);

	mesh->connections = NULL;
	mesh->everyone = NULL;
}

connection_t *new_connection(void) {
	return xzalloc(sizeof(connection_t));
}

void free_connection(connection_t *c) {
	if(!c) {
		return;
	}

	sptps_stop(&c->sptps);
	ecdsa_free(c->ecdsa);

	buffer_clear(&c->inbuf);
	buffer_clear(&c->outbuf);

	if(c->io.cb) {
		abort();
	}

	if(c->socket > 0) {
		closesocket(c->socket);
	}

	free(c->name);

	free(c);
}

void connection_add(meshlink_handle_t *mesh, connection_t *c) {
	c->mesh = mesh;
	list_insert_tail(mesh->connections, c);
}

void connection_del(meshlink_handle_t *mesh, connection_t *c) {
	io_del(&mesh->loop, &c->io);
	list_delete(mesh->connections, c);
}
