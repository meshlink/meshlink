/*
    node_sim_peer.c -- Implementation of Node Simulation for Meshlink Testing
                    for meta connection test case 01 - re-connection of
                    two nodes when relay node goes down
    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>

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
#include <time.h>
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/network_namespace_framework.h"
#include "../../utils.h"
#include "../run_blackbox_tests/test_optimal_pmtu.h"

extern bool test_pmtu_peer_running;

static struct sync_flag nut_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag channel_opened = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len);

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;

	assert(port == CHANNEL_PORT);

	if(!strcmp(channel->node->name, "nut")) {
		meshlink_set_channel_receive_cb(mesh, channel, channel_receive_cb);
		//channel->node->priv = channel;

		return true;
	}

	return false;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	assert(meshlink_channel_send(mesh, channel, "test", 5) >= 0);
	return;
}

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	if(len == 0) {
		// channel closed
		fail();
		return;
	}

	if(!strcmp(channel->node->name, "nut")) {
		if(!memcmp(dat, "reply", 5)) {
			set_sync_flag(&channel_opened, true);
		} else if(!memcmp(dat, "test", 5)) {
			assert(meshlink_channel_send(mesh, channel, "reply", 5) >= 0);
		}
	}

	return;
}

static void log_message(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)mesh;

	if(level == MESHLINK_INFO) {
		fprintf(stderr, "\x1b[34m peer:\x1b[0m %s\n", text);
	}
}

void *node_sim_pmtu_peer_01(void *arg) {
	mesh_arg_t *mesh_arg = (mesh_arg_t *)arg;
	struct timeval main_loop_wait = { 5, 0 };

	// Run relay node instance


	meshlink_handle_t *mesh;
	mesh = meshlink_open(mesh_arg->node_name , mesh_arg->confbase, mesh_arg->app_name, mesh_arg->dev_class);
	assert(mesh);

	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_message);
	meshlink_set_channel_accept_cb(mesh, channel_accept);

	if(mesh_arg->join_invitation) {
		int attempts;
		bool join_ret;

		for(attempts = 0; attempts < 10; attempts++) {
			join_ret = meshlink_join(mesh, mesh_arg->join_invitation);

			if(join_ret) {
				break;
			}

			sleep(1);
		}

		if(attempts == 10) {
			abort();
		}
	}

	assert(meshlink_start(mesh));

	// All test steps executed - wait for signals to stop/start or close the mesh

	while(test_pmtu_peer_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);
	}

	meshlink_close(mesh);

	return NULL;
}
