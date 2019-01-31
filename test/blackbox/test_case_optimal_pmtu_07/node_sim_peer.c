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
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/mesh_event_handler.h"
#include "../../utils.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_DEVCLASS   2
#define CMD_LINE_ARG_CLIENTID   3
#define CMD_LINE_ARG_IMPORTSTR  4
#define CMD_LINE_ARG_INVITEURL  5
#define CHANNEL_PORT 1234

static struct sync_flag nut_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag channel_opened = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len);

static int client_id = -1;

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                           bool reachable) {
	if(!strcasecmp(node->name, "nut") && reachable) {
		//set_sync_flag(&nut_reachable, true);
		mesh_event_sock_send(client_id, reachable ? NODE_JOINED : NODE_LEFT, node->name, 100);
	}

	return;
}

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
		mesh_event_sock_send(client_id, ERR_NETWORK, channel->node->name, 100);
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

int main(int argc, char *argv[]) {
	struct timeval main_loop_wait = { 2, 0 };

	// Import mesh event handler

	if((argv[CMD_LINE_ARG_CLIENTID]) && (argv[CMD_LINE_ARG_IMPORTSTR])) {
		client_id = atoi(argv[CMD_LINE_ARG_CLIENTID]);
		mesh_event_sock_connect(argv[CMD_LINE_ARG_IMPORTSTR]);
	}

	// Setup required signals

	setup_signals();

	// Run peer node instance

	meshlink_handle_t *mesh = meshlink_open("testconf", argv[CMD_LINE_ARG_NODENAME],
	                                        "test_channel_conn", atoi(argv[CMD_LINE_ARG_DEVCLASS]));
	assert(mesh);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, meshlink_callback_logger);
	meshlink_set_channel_accept_cb(mesh, channel_accept);
	meshlink_enable_discovery(mesh, false);

	if(argv[CMD_LINE_ARG_INVITEURL]) {
		int attempts;
		bool join_ret;

		for(attempts = 0; attempts < 10; attempts++) {
			join_ret = meshlink_join(mesh, argv[CMD_LINE_ARG_INVITEURL]);

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
	mesh_event_sock_send(client_id, NODE_STARTED, NULL, 0);

	//assert(wait_sync_flag(&nut_reachable, 10));

	/*meshlink_node_t *nut_node = meshlink_get_node(mesh, "nut");
	assert(nut_node);
	meshlink_channel_t *channel = meshlink_channel_open(mesh, nut_node, CHANNEL_PORT,
	                              channel_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);

	assert(wait_sync_flag(&channel_opened, 20));
	mesh_event_sock_send(client_id, NODE_STARTED, NULL, 0);*/

	// All test steps executed - wait for signals to stop/start or close the mesh

	while(test_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);
	}

	meshlink_close(mesh);

	return EXIT_SUCCESS;
}
