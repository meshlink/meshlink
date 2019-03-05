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
static struct sync_flag app_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER, .flag = false};
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

	fprintf(stderr, "\tapp2node2 got channel request from %s\n", channel->node->name);

	if(!strcmp(channel->node->name, "corenode1")) {
		meshlink_set_channel_receive_cb(mesh, channel, channel_receive_cb);
		mesh->priv = channel;

		return true;
	}

	return false;
}

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	char data[100] = {0};

	if(len == 0) {
		fprintf(stderr, "\tapp2node2 got error from %s at %lu\n", channel->node->name, time(NULL));
		send_event(ERR_NETWORK);
		return;
	}

	memcpy(data, dat, len);

	fprintf(stderr, "\tapp2node2 got message from %s as %s\n", channel->node->name, data);

	if(!strcmp(channel->node->name, "corenode1")) {
		if(!memcmp(dat, "Channel Message", len)) {
			set_sync_flag(&channel_data_recieved, true);
		} else if(!memcmp(dat, "failure", 7)) {
			assert(false);
		}
	} else if(!strcmp(channel->node->name, "app2node1")) {
		if(!memcmp(dat, "Channel Message", len)) {
			set_sync_flag(&channel_data_recieved, true);
		} else if(!memcmp(dat, "failure", 7)) {
			assert(false);
		}
	} else {
		assert(false);
	}

	return;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	char *message = "Channel Message";
	char *node = (char *)channel->node->name;
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	fprintf(stderr, "\tapp2node2's Channel request has been accepted by %s at : %lu\n", node, time(NULL));
	set_sync_flag(&channel_opened, true);
	assert(meshlink_channel_send(mesh, channel, message, strlen(message)) >= 0);
	return;
}


static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                           bool reachable) {
	if(!strcasecmp(node->name, "corenode1")) {
		if(reachable) {
			fprintf(stderr, "\tNode corenode1 became reachable\n");
			set_sync_flag(&peer_reachable, true);
		}
	} else if(!strcasecmp(node->name, "app2node1")) {
		if(reachable) {
			fprintf(stderr, "\tNode app2node1 became reachable\n");
			set_sync_flag(&app_reachable, true);
		}
	}

	return;
}

void mesh_start_test_handler(int a) {
	fprintf(stderr, "Starting test in app2node2\n");
	set_sync_flag(&start_test, true);
}

int main(int argc, char *argv[]) {
	size_t num_nodes, i;
	struct timeval main_loop_wait = { 2, 0 };
	meshlink_channel_t *channel = NULL;
	meshlink_node_t *core_node = NULL;
	meshlink_node_t **node_handles = NULL;

	fprintf(stderr, "\tMesh node 'app2node2' starting up........\n");

	// Import mesh event handler

	if((argv[CMD_LINE_ARG_CLIENTID]) && (argv[CMD_LINE_ARG_IMPORTSTR])) {
		client_id = atoi(argv[CMD_LINE_ARG_CLIENTID]);
		mesh_event_sock_connect(argv[CMD_LINE_ARG_IMPORTSTR]);
	}

	// Setup required signals

	setup_signals();
	signal(SIGIO, mesh_start_test_handler);

	// Run peer node instance

	mesh = meshlink_open("app2node2conf", argv[CMD_LINE_ARG_NODENAME],
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
	fprintf(stderr, "\tapp2node2 Sending Channel request to corenode1 at : %lu\n", time(NULL));
	channel = meshlink_channel_open(mesh, core_node, CHANNEL_PORT,
	                                channel_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);
	assert(wait_sync_flag(&channel_opened, 30));
	send_event(CHANNEL_OPENED);

	assert(wait_sync_flag(&channel_data_recieved, 30));
	send_event(CHANNEL_DATA_RECIEVED);

	// Open a channel to peer node
	channel_opened.flag = false;
	channel_data_recieved.flag = false;

	assert(wait_sync_flag(&app_reachable, 60));

	core_node = meshlink_get_node(mesh, "app2node1");
	assert(core_node);
	fprintf(stderr, "\tapp2node2 Sending Channel request to app2node1 at : %lu\n", time(NULL));
	channel = meshlink_channel_open(mesh, core_node, CHANNEL_PORT,
	                                channel_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);
	assert(wait_sync_flag(&channel_opened, 15));
	send_event(CHANNEL_OPENED);

	assert(wait_sync_flag(&channel_data_recieved, 30));
	send_event(CHANNEL_DATA_RECIEVED);

	num_nodes = 0;
	node_handles = meshlink_get_all_nodes(mesh, NULL, &num_nodes);
	fprintf(stderr, "\tGot %d nodes in list with error : %s\n", num_nodes, meshlink_strerror(meshlink_errno));
	assert(node_handles);
	assert((num_nodes == 4));

	for(i = 0; i < num_nodes; i++) {
		fprintf(stderr, "\tChecking the node : %s\n", node_handles[i]->name);

		if(0 == strcmp(node_handles[i]->name, "app1node1")) {
			send_event(SIG_ABORT);
			assert(false);
		} else if(0 == strcmp(node_handles[i]->name, "app1node2")) {
			send_event(SIG_ABORT);
			assert(false);
		}
	}

	meshlink_node_t *node = meshlink_get_self(mesh);
	assert(node);
	meshlink_submesh_t *submesh = meshlink_get_node_submesh(mesh, node);
	assert(submesh);

	node_handles = meshlink_get_all_nodes_by_submesh(mesh, submesh, node_handles, &num_nodes);
	assert(node_handles);
	assert((num_nodes == 2));

	for(i = 0; i < num_nodes; i++) {
		fprintf(stderr, "\tChecking the node : %s\n", node_handles[i]->name);

		if((0 == strcmp(node_handles[i]->name, "app1node1")) || (0 == strcmp(node_handles[i]->name, "app1node2"))) {
			send_event(SIG_ABORT);
			assert(false);
		}
	}

	send_event(MESH_EVENT_COMPLETED);

	// All test steps executed - wait for signals to stop/start or close the mesh

	while(test_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);
	}

	meshlink_close(mesh);

	return EXIT_SUCCESS;
}