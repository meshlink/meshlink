/*
    node_sim_relay.c -- Implementation of Node Simulation for Meshlink Testing
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
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/network_namespace_framework.h"

bool test_channel_blacklist_disonnection_relay_01_running;

void *test_channel_blacklist_disonnection_relay_01(void *arg) {
	struct timeval main_loop_wait = { 2, 0 };
	mesh_arg_t *mesh_arg = (mesh_arg_t *)arg;
	test_channel_blacklist_disonnection_relay_01_running = true;

	// Run relay node instance

	meshlink_handle_t *mesh;
	mesh = meshlink_open(mesh_arg->node_name, mesh_arg->confbase, mesh_arg->app_name, mesh_arg->dev_class);
	assert(mesh);

	meshlink_start(mesh);

	/* All test steps executed - wait for signals to stop/start or close the mesh */
	while(test_channel_blacklist_disonnection_relay_01_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);
	}

	meshlink_close(mesh);

	return NULL;
}
