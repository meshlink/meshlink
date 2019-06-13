/*
    node_sim_peer.c -- Implementation of Node Simulation for Meshlink Testing
                    for channel connections with respective to blacklisting their nodes
    Copyright (C) 2019  Guus Sliepen <guus@meshlink.io>

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/network_namespace_framework.h"
#include "../../utils.h"

#define CHANNEL_PORT 1234

bool test_channel_blacklist_disonnection_peer_01_running;
bool test_case_signal_peer_restart_01;

static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len);

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;

	assert(port == CHANNEL_PORT);

	if(!strcmp(channel->node->name, "nut")) {
		meshlink_set_channel_receive_cb(mesh, channel, channel_receive_cb);
		return true;
	}

	return false;
}

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {

	if(len == 0) {
		fprintf(stderr, "Channel closure\n");
	}

	if(!strcmp(channel->node->name, "nut")) {
		if(!memcmp(dat, "test", 5)) {
			assert(meshlink_channel_send(mesh, channel, "reply", 5) >= 0);
		}
	}

	return;
}

void *test_channel_blacklist_disonnection_peer_01(void *arg) {
	struct timeval main_loop_wait = { 2, 0 };
	mesh_arg_t *mesh_arg = (mesh_arg_t *)arg;
	test_channel_blacklist_disonnection_peer_01_running = true;

	// Run relay node instance

	meshlink_handle_t *mesh;
	mesh = meshlink_open(mesh_arg->node_name, mesh_arg->confbase, mesh_arg->app_name, mesh_arg->dev_class);
	assert(mesh);
	meshlink_set_channel_accept_cb(mesh, channel_accept);

	// Join relay node and if fails to join then try few more attempts

	if(mesh_arg->join_invitation) {
		assert(meshlink_join(mesh, mesh_arg->join_invitation));
	}

	assert(meshlink_start(mesh));

	// All test steps executed - wait for signals to stop/start or close the mesh

	while(test_channel_blacklist_disonnection_peer_01_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);

		if(test_case_signal_peer_restart_01) {
			meshlink_stop(mesh);
			assert(meshlink_start(mesh));
			test_case_signal_peer_restart_01 = false;
		}
	}

	meshlink_close(mesh);

	return NULL;
}
