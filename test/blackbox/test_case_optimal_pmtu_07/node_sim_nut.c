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
#include <time.h>
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/mesh_event_handler.h"
#include "../../utils.h"
#include "../run_blackbox_tests/test_optimal_pmtu.h"

#define CMD_LINE_ARG_NODENAME   1
#define CMD_LINE_ARG_DEVCLASS   2
#define CMD_LINE_ARG_CLIENTID   3
#define CMD_LINE_ARG_IMPORTSTR  4
#define CMD_LINE_ARG_INVITEURL  5
#define CHANNEL_PORT 1234

#pragma pack(1)

static int client_id = -1;

static struct sync_flag peer_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag channel_opened = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                           bool reachable);
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len);

static pmtu_attr_t node_pmtu[2];

static void print_mtu_calc(pmtu_attr_t node_pmtu) {
	fprintf(stderr, "MTU size : %d\n", node_pmtu.mtu_size);
	fprintf(stderr, "Probes took for calculating PMTU discovery : %d\n", node_pmtu.mtu_discovery.probes);
	fprintf(stderr, "Probes total length took for calculating PMTU discovery : %d\n", node_pmtu.mtu_discovery.probes_total_len);
	fprintf(stderr, "Time took for calculating PMTU discovery : %lu\n", node_pmtu.mtu_discovery.time);
	fprintf(stderr, "Total MTU ping probes : %d\n", node_pmtu.mtu_ping.probes);
	fprintf(stderr, "Total MTU ping probes length : %d\n", node_pmtu.mtu_ping.probes_total_len);
	float avg = 0;

	if(node_pmtu.mtu_ping.probes) {
		avg = (float)node_pmtu.mtu_ping.time / (float)node_pmtu.mtu_ping.probes;
	}

	fprintf(stderr, "Average MTU ping probes ping time : %f\n", avg);
	fprintf(stderr, "Total probes received %d\n", node_pmtu.mtu_recv_probes.probes);
	fprintf(stderr, "Total probes sent %d\n", node_pmtu.mtu_sent_probes.probes);
}

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                           bool reachable) {
	if(!strcasecmp(node->name, "peer") && reachable) {
		set_sync_flag(&peer_reachable, true);
	}

	mesh_event_sock_send(client_id, reachable ? NODE_JOINED : NODE_LEFT, node->name, 100);
	return;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	assert(meshlink_channel_send(mesh, channel, "test", 5) >= 0);
	return;
}

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;

	assert(port == CHANNEL_PORT);

	if(!strcmp(channel->node->name, "peer")) {
		meshlink_set_channel_receive_cb(mesh, channel, channel_receive_cb);
		mesh->priv = channel;

		return true;
	}

	return false;
}

/* channel receive callback */
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	if(len == 0) {
		mesh_event_sock_send(client_id, ERR_NETWORK, channel->node->name, 100);
		return;
	}

	if(!strcmp(channel->node->name, "peer")) {
		if(!memcmp(dat, "reply", 5)) {
			set_sync_flag(&channel_opened, true);
			fprintf(stderr, "GOT REPLY FROM PEER\n");
		} else if(!memcmp(dat, "test", 5)) {
			assert(meshlink_channel_send(mesh, channel, "reply", 5) >= 0);
		}
	}

	return;
}

void meshlink_logger(meshlink_handle_t *mesh, meshlink_log_level_t level,
                     const char *text) {
	(void)mesh;
	(void)level;
	int probe_len;
	int mtu_len;
	int probes;
	char node_name[100];
	int i = -1;

	time_t cur_time;
	time_t probe_interval;

	cur_time = time(NULL);
	assert(cur_time != -1);

	static time_t node_shutdown_time = 0;
	bool mtu_probe = false;

	if(node_shutdown_time && cur_time >= node_shutdown_time) {
		test_running = false;
	}

	static const char *levelstr[] = {
		[MESHLINK_DEBUG] = "\x1b[34mDEBUG",
		[MESHLINK_INFO] = "\x1b[32mINFO",
		[MESHLINK_WARNING] = "\x1b[33mWARNING",
		[MESHLINK_ERROR] = "\x1b[31mERROR",
		[MESHLINK_CRITICAL] = "\x1b[31mCRITICAL",
	};

	fprintf(stderr, "%s:\x1b[0m %s\n", levelstr[level], text);

	if(sscanf(text, "Sending MTU probe length %d to %s", &probe_len, node_name) == 2) {
		find_node_index(i, node_name);
		node_pmtu[i].mtu_sent_probes.probes += 1;
		node_pmtu[i].mtu_sent_probes.probes_total_len += probe_len;

		if(node_pmtu[i].mtu_size) {
			if(node_pmtu[i].mtu_sent_probes.time > node_pmtu[i].mtu_recv_probes.time) {
				probe_interval = cur_time - node_pmtu[i].mtu_sent_probes.time;
			} else {
				probe_interval = cur_time - node_pmtu[i].mtu_recv_probes.time;
			}

			node_pmtu[i].mtu_ping.probes += 1;
			node_pmtu[i].mtu_ping.time += probe_interval;
			node_pmtu[i].mtu_ping.probes_total_len += probe_len;
		}

		node_pmtu[i].mtu_sent_probes.time = cur_time;

	} else if(sscanf(text, "Got MTU probe length %d from %s", &probe_len, node_name) == 2) {
		find_node_index(i, node_name);
		node_pmtu[i].mtu_recv_probes.probes += 1;
		node_pmtu[i].mtu_recv_probes.probes_total_len += probe_len;

		if(node_pmtu[i].mtu_size) {
			if(node_pmtu[i].mtu_sent_probes.time > node_pmtu[i].mtu_recv_probes.time) {
				probe_interval = cur_time - node_pmtu[i].mtu_sent_probes.time;
			} else {
				probe_interval = cur_time - node_pmtu[i].mtu_recv_probes.time;
			}

			node_pmtu[i].mtu_ping.probes += 1;
			node_pmtu[i].mtu_ping.time += probe_interval;
			node_pmtu[i].mtu_ping.probes_total_len += probe_len;
		}

		node_pmtu[i].mtu_recv_probes.time = cur_time;

	} else if(sscanf(text, "Fixing MTU of %s to %d after %d probes", node_name, &mtu_len, &probes) == 3) {
		static bool mtu_set = true;

		if(!node_shutdown_time && !strcasecmp("relay", node_name) && mtu_set) {
			node_shutdown_time = cur_time + PING_TRACK_TIMEOUT;
			mtu_set = false;
		}

		find_node_index(i, node_name);
		node_pmtu[i].mtu_discovery.probes = node_pmtu[i].mtu_recv_probes.probes + node_pmtu[i].mtu_sent_probes.probes;
		node_pmtu[i].mtu_discovery.probes_total_len = node_pmtu[i].mtu_sent_probes.probes_total_len + node_pmtu[i].mtu_recv_probes.probes_total_len;
		node_pmtu[i].mtu_discovery.time = cur_time - node_pmtu[i].mtu_start.time;
		node_pmtu[i].mtu_discovery.count += 1;
		node_pmtu[i].mtu_size = mtu_len;

	} else if(sscanf(text, "SPTPS key exchange with %s succesful", node_name) == 1) {
		find_node_index(i, node_name);
		node_pmtu[i].mtu_start.time = cur_time;
		node_pmtu[i].mtu_start.count += 1;
		memset(&node_pmtu[i].mtu_discovery, 0, sizeof(struct pmtu_attr_para));
		memset(&node_pmtu[i].mtu_ping, 0, sizeof(struct pmtu_attr_para));
		memset(&node_pmtu[i].mtu_increase, 0, sizeof(struct pmtu_attr_para));

	} else if(sscanf(text, "Increase in PMTU to %s detected, restarting PMTU discovery", node_name) == 1) {
		find_node_index(i, node_name);
		node_pmtu[i].mtu_increase.time = cur_time - node_pmtu[i].mtu_start.time;
		node_pmtu[i].mtu_increase.count += 1;

	} else if(sscanf(text, "Trying to send MTU probe to unreachable or rekeying node %s", node_name) == 1) {

	} else if(sscanf(text, "%s did not respond to UDP ping, restarting PMTU discovery", node_name) == 1) {

	} else if(sscanf(text, "No response to MTU probes from %s", node_name) == 1) {

	} else if((sscanf(text, "Connection with %s activated", node_name) == 1) || (sscanf(text, "Already connected to %s", node_name) == 1)) {
		mesh_event_sock_send(client_id, META_CONN_SUCCESSFUL, node_name, sizeof(node_name));

	} else if((sscanf(text, "Connection closed by %s", node_name) == 1) || (sscanf(text, "Closing connection with %s", node_name) == 1)) {
		mesh_event_sock_send(client_id, META_CONN_CLOSED, node_name, sizeof(node_name));

	}
}

int main(int argc, char *argv[]) {
	struct timeval main_loop_wait = { 5, 0 };
	int i;

	// Import mesh event handler

	if((argv[CMD_LINE_ARG_CLIENTID]) && (argv[CMD_LINE_ARG_IMPORTSTR])) {
		client_id = atoi(argv[CMD_LINE_ARG_CLIENTID]);
		mesh_event_sock_connect(argv[CMD_LINE_ARG_IMPORTSTR]);
	}

	setup_signals();

	// Execute test steps

	meshlink_handle_t *mesh = meshlink_open("testconf", argv[CMD_LINE_ARG_NODENAME],
	                                        "test_channel_conn", atoi(argv[CMD_LINE_ARG_DEVCLASS]));
	assert(mesh);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, meshlink_logger);
	meshlink_set_node_status_cb(mesh, node_status_cb);
	meshlink_enable_discovery(mesh, false);
	sleep(1);

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

	// Wait for peer node to join

	assert(wait_sync_flag(&peer_reachable, 10));

	// Open a channel to peer node

	meshlink_node_t *peer_node = meshlink_get_node(mesh, "peer");
	assert(peer_node);
	meshlink_channel_t *channel = meshlink_channel_open(mesh, peer_node, CHANNEL_PORT,
	                              channel_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);

	assert(wait_sync_flag(&channel_opened, 30));
	assert(mesh_event_sock_send(client_id, CHANNEL_OPENED, NULL, 0));

	// All test steps executed - wait for signals to stop/start or close the mesh

	time_t time_stamp, send_time;

	time_stamp = time(NULL);
	send_time = time_stamp + 10;

	while(test_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);

		time_stamp = time(NULL);

		if(time_stamp >= send_time) {
			send_time = time_stamp + 10;
			meshlink_channel_send(mesh, channel, "ping", 5);
		}
	}

	pmtu_attr_t send_mtu_data;
	send_mtu_data = node_pmtu[NODE_PMTU_PEER];
	print_mtu_calc(send_mtu_data);
	assert(mesh_event_sock_send(client_id, OPTIMAL_PMTU_PEER, &send_mtu_data, sizeof(send_mtu_data)));
	send_mtu_data = node_pmtu[NODE_PMTU_RELAY];
	print_mtu_calc(send_mtu_data);
	assert(mesh_event_sock_send(client_id, OPTIMAL_PMTU_RELAY, &send_mtu_data, sizeof(send_mtu_data)));

	meshlink_close(mesh);
}
