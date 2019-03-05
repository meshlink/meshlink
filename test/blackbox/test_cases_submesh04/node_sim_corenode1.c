/*
    node_sim.c -- Implementation of Node Simulation for Meshlink Testing
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

static bool conn_status = false;
static int client_id = -1;

static struct sync_flag peer_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};
static struct sync_flag channel_opened = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};
static struct sync_flag channel_data_recieved = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};

static meshlink_handle_t *mesh = NULL;

static void mesh_send_message_handler(char *destination);

static void send_event(mesh_event_t event) {
	int attempts;

	for(attempts = 0; attempts < 5; attempts += 1) {
		if(mesh_event_sock_send(client_id, event, NULL, 0)) {
			break;
		}
	}

	assert(attempts < 5);

	return;
}

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	char data[100] = {0};

	if(len == 0) {
		send_event(ERR_NETWORK);
		return;
	}

	memcpy(data, dat, len);

	fprintf(stderr, "corenode1 got message from %s as %s\n", channel->node->name, data);

	if(!memcmp(dat, "Channel Message", len)) {
		mesh_send_message_handler((char *)channel->node->name);

		if(0 == strcmp("app1node2", channel->node->name)) {
			set_sync_flag(&channel_data_recieved, true);
		}
	} else if(!memcmp(dat, "failure", 7)) {
		assert(false);
	}

	return;
}

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                           bool reachable) {
	if(reachable) {
		fprintf(stderr, "Node %s became reachable\n", node->name);
	} else {
		fprintf(stderr, "Node %s is unreachable\n", node->name);
	}

	return;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	char *message = "Channel Message";
	char *node = (char *)channel->node->name;
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	fprintf(stderr, "corenode1's Channel request has been accepted by %s at : %lu\n", node, time(NULL));

	if(0 == strcmp("app1node2", node)) {
		set_sync_flag(&channel_opened, true);
	}

	assert(meshlink_channel_send(mesh, channel, message, strlen(message)) >= 0);
	return;
}

/* channel receive callback */
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;

	assert(port == CHANNEL_PORT);

	fprintf(stderr, "corenode1 got channel request from %s\n", channel->node->name);
	meshlink_set_channel_receive_cb(mesh, channel, channel_receive_cb);

	return true;
}

void mesh_send_message_handler(char *destination) {
	meshlink_channel_t *channel = NULL;
	meshlink_node_t *target_node = NULL;

	// Open a channel to destination node
	target_node = meshlink_get_node(mesh, destination);
	assert(target_node);
	fprintf(stderr, "corenode1 Sending Channel request to %s at : %lu\n", destination, time(NULL));
	channel = meshlink_channel_open(mesh, target_node, CHANNEL_PORT,
	                                channel_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);
}

int main(int argc, char *argv[]) {
	struct timeval main_loop_wait = { 5, 0 };
	int i;

	// Import mesh event handler

	fprintf(stderr, "Mesh node 'corenode1' starting up........\n");

	if((argv[CMD_LINE_ARG_CLIENTID]) && (argv[CMD_LINE_ARG_IMPORTSTR])) {
		client_id = atoi(argv[CMD_LINE_ARG_CLIENTID]);
		mesh_event_sock_connect(argv[CMD_LINE_ARG_IMPORTSTR]);
	}

	setup_signals();

	// Execute test steps

	mesh = meshlink_open("testconf", argv[CMD_LINE_ARG_NODENAME],
	                     "test_channel_conn", atoi(argv[CMD_LINE_ARG_DEVCLASS]));
	assert(mesh);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, meshlink_callback_logger);
	meshlink_set_channel_accept_cb(mesh, channel_accept);
	meshlink_set_node_status_cb(mesh, node_status_cb);

	if(argv[CMD_LINE_ARG_INVITEURL]) {
		assert(meshlink_join(mesh, argv[CMD_LINE_ARG_INVITEURL]));
	}

	assert(meshlink_start(mesh));

	send_event(NODE_STARTED);

	assert(wait_sync_flag(&channel_opened, 50));
	send_event(CHANNEL_OPENED);

	assert(wait_sync_flag(&channel_data_recieved, 50));
	send_event(CHANNEL_DATA_RECIEVED);

	// All test steps executed - wait for signals to stop/start or close the mesh

	while(test_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);
	}

	meshlink_close(mesh);

	return 0;
}
