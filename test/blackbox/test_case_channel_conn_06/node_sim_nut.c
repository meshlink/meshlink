/*
    node_sim_nut.c -- Implementation of Node Simulation for Meshlink Testing
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

static int client_id = -1;

static struct sync_flag peer_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag channel_opened = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag channel_closed = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag sigusr_received = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static void send_event(mesh_event_t event);
static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable);

static void mesh_siguser1_signal_handler(int sig_num) {
	(void)sig_num;

	set_sync_flag(&sigusr_received, true);
	return;
}

static void send_event(mesh_event_t event) {
	bool send_ret = false;
	int attempts;

	for(attempts = 0; attempts < 5; attempts += 1) {
		send_ret = mesh_event_sock_send(client_id, event, NULL, 0);

		if(send_ret) {
			break;
		}
	}

	return;
}

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;


	if(!strcasecmp(node->name, "peer") && reachable) {
		set_sync_flag(&peer_reachable, true);
	}

	return;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	assert(meshlink_channel_send(mesh, channel, "test", 5) >= 0);
	return;
}

static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	(void)mesh;


	if(len == 0) {
		set_sync_flag(&channel_closed, true);
		send_event(ERR_NETWORK);
		return;
	}

	if(!strcmp(channel->node->name, "peer")) {
		if(len == 5 && !memcmp(dat, "reply", 5)) {
			set_sync_flag(&channel_opened, true);
		}
	}

	return;
}

int main(int argc, char *argv[]) {
	(void)argc;

	struct timeval main_loop_wait = { 2, 0 };

	// Import mesh event handler

	if((argv[CMD_LINE_ARG_CLIENTID]) && (argv[CMD_LINE_ARG_IMPORTSTR])) {
		client_id = atoi(argv[CMD_LINE_ARG_CLIENTID]);
		mesh_event_sock_connect(argv[CMD_LINE_ARG_IMPORTSTR]);
	}

	// Setup required signals

	setup_signals();
	signal(SIGUSR1, mesh_siguser1_signal_handler);

	// Execute test steps

	meshlink_handle_t *mesh = meshlink_open("testconf", argv[CMD_LINE_ARG_NODENAME],
	                                        "test_channel_conn", atoi(argv[CMD_LINE_ARG_DEVCLASS]));
	assert(mesh);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, meshlink_callback_logger);
	meshlink_set_node_status_cb(mesh, node_status_cb);

	if(argv[CMD_LINE_ARG_INVITEURL]) {
		assert(meshlink_join(mesh, argv[CMD_LINE_ARG_INVITEURL]));
	}

	assert(meshlink_start(mesh));

	// Wait for peer node to join

	assert(wait_sync_flag(&peer_reachable, 30));
	send_event(NODE_JOINED);

	// Open a channel to peer node

	meshlink_node_t *peer_node = meshlink_get_node(mesh, "peer");
	assert(peer_node);
	meshlink_channel_t *channel = meshlink_channel_open(mesh, peer_node, CHANNEL_PORT,
	                              channel_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);

	assert(wait_sync_flag(&channel_opened, 10));
	send_event(CHANNEL_OPENED);
	assert(wait_sync_flag(&sigusr_received, 10));

	sleep(40);
	assert(meshlink_channel_send(mesh, channel, "after", 6) >= 0);


	assert(wait_sync_flag(&channel_closed, 140));


	// All test steps executed - wait for signals to stop/start or close the mesh
	while(test_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);
	}

	meshlink_close(mesh);
}
