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
#include "../common/mesh_event_handler.h"
#include "../../utils.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_DEVCLASS   2
#define CMD_LINE_ARG_CLIENTID   3
#define CMD_LINE_ARG_IMPORTSTR  4
#define CMD_LINE_ARG_INVITEURL  5
#define CHANNEL_PORT 1234

static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len);

static int client_id = -1;
static meshlink_handle_t *mesh = NULL;

static struct sync_flag peer_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};
static struct sync_flag start_test = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};
static struct sync_flag channel_opened = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};
static struct sync_flag channel_data_recieved = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};

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

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;

	assert(port == CHANNEL_PORT);

	fprintf(stderr, "\tapp1node1 got channel request from %s\n", channel->node->name);

	if(!strcmp(channel->node->name, "corenode1")) {
		fprintf(stderr, "\tapp1node1 accepting channel request from %s at %lu\n", channel->node->name, time(NULL));
		meshlink_set_channel_receive_cb(mesh, channel, channel_receive_cb);
		mesh->priv = channel;

		return true;
	} else if(!strcmp(channel->node->name, "app1node2")) {
		fprintf(stderr, "\tapp1node1 accepting channel request from %s at %lu\n", channel->node->name, time(NULL));
		meshlink_set_channel_receive_cb(mesh, channel, channel_receive_cb);
		mesh->priv = channel;

		return true;
	}

	fprintf(stderr, "\tapp1node1 rejecting channel request from %s at %lu\n", channel->node->name, time(NULL));
	return false;
}

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	char data[100] = {0};
	char *message = "Channel Message";

	if(len == 0) {
		fprintf(stderr, "\tapp1node1 got error from %s at %lu\n", channel->node->name, time(NULL));
		send_event(ERR_NETWORK);
		return;
	}

	memcpy(data, dat, len);

	fprintf(stderr, "\tapp1node1 got message from %s as %s\n", channel->node->name, data);

	if(!strcmp(channel->node->name, "corenode1")) {
		if(!memcmp(dat, "Channel Message", len)) {
			set_sync_flag(&channel_data_recieved, true);
		} else if(!memcmp(dat, "failure", 7)) {
			assert(false);
		}
	} else if(!strcmp(channel->node->name, "app1node2")) {
		if(!memcmp(dat, "Channel Message", len)) {
			assert(meshlink_channel_send(mesh, channel, message, strlen(message)) >= 0);
		} else if(!memcmp(dat, "failure", 7)) {
			assert(false);
		}
	}

	return;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	char *message = "Channel Message";
	char *node = (char *)channel->node->name;
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	fprintf(stderr, "\tapp1node1's Channel request has been accepted by %s at : %lu\n", node, time(NULL));

	if(0 == strcmp("corenode1", node)) {
		set_sync_flag(&channel_opened, true);
	}

	assert(meshlink_channel_send(mesh, channel, message, strlen(message)) >= 0);
	return;
}


static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!strcasecmp(node->name, "corenode1")) {
		if(reachable) {
			fprintf(stderr, "\tNode corenode1 became reachable\n");
			set_sync_flag(&peer_reachable, true);
		}
	}

	return;
}

void mesh_start_test_handler(int signum) {
	(void)signum;

	fprintf(stderr, "Starting test in app1node1\n");
	set_sync_flag(&start_test, true);
}

int main(int argc, char *argv[]) {
	(void)argc;

	struct timeval main_loop_wait = { 2, 0 };
	meshlink_channel_t *channel = NULL;
	meshlink_node_t *core_node = NULL;

	fprintf(stderr, "\tMesh node 'app1node1' starting up........\n");

	// Import mesh event handler

	if((argv[CMD_LINE_ARG_CLIENTID]) && (argv[CMD_LINE_ARG_IMPORTSTR])) {
		client_id = atoi(argv[CMD_LINE_ARG_CLIENTID]);
		mesh_event_sock_connect(argv[CMD_LINE_ARG_IMPORTSTR]);
	}

	// Setup required signals

	setup_signals();
	signal(SIGIO, mesh_start_test_handler);

	// Run peer node instance

	mesh = meshlink_open("app1node1conf", argv[CMD_LINE_ARG_NODENAME],
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

	// Wait for peer node to join

	assert(wait_sync_flag(&peer_reachable, 15));
	send_event(NODE_JOINED);

	while(false == wait_sync_flag(&start_test, 10));

	// Open a channel to peer node
	core_node = meshlink_get_node(mesh, "corenode1");
	assert(core_node);
	fprintf(stderr, "\tapp1node1 Sending Channel request to corenode1 at : %lu\n", time(NULL));
	channel = meshlink_channel_open(mesh, core_node, CHANNEL_PORT,
	                                channel_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);
	assert(wait_sync_flag(&channel_opened, 15));
	send_event(CHANNEL_OPENED);

	assert(wait_sync_flag(&channel_data_recieved, 30));
	send_event(CHANNEL_DATA_RECIEVED);

	// All test steps executed - wait for signals to stop/start or close the mesh

	while(test_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);
	}

	meshlink_close(mesh);

	return EXIT_SUCCESS;
}
