/*
    node_sim_nut.c -- Implementation of Node Simulation for Meshlink Testing
                    for meta connection test case 01 - re-connection of
                    two nodes when relay node goes down
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
#include <time.h>
#include "../common/common_handlers.h"
#include "../common/test_step.h"
#include "../common/network_namespace_framework.h"
#include "../../utils.h"
#include "../run_blackbox_tests/test_optimal_pmtu.h"

extern bool test_pmtu_nut_running;
extern bool test_pmtu_peer_running;
extern bool test_pmtu_relay_running;
extern struct sync_flag test_pmtu_nut_closed;
extern bool ping_channel_enable_07;

static struct sync_flag peer_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag channel_opened = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                           bool reachable);
static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len);
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len);

pmtu_attr_t node_pmtu[2];
static time_t node_shutdown_time = 0;
static bool mtu_set = true;

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

// Node status callback
static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                           bool reachable) {
	// Signal pthread_cond_wait if peer is reachable
	if(!strcasecmp(node->name, "peer") && reachable) {
		set_sync_flag(&peer_reachable, true);
	}

	return;
}

// Channel poll callback
static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	// Send data via channel to trigger UDP peer to peer hole punching
	assert(meshlink_channel_send(mesh, channel, "test", 5) >= 0);
	return;
}

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *dat, size_t len) {
	(void)dat;
	(void)len;

	assert(port == CHANNEL_PORT);

	// If the channel is from peer node set receive callback for it else reject the channel
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
		fail();
		return;
	}

	if(!strcmp(channel->node->name, "peer")) {
		if(!memcmp(dat, "reply", 5)) {
			set_sync_flag(&channel_opened, true);
		} else if(!memcmp(dat, "test", 5)) {
			assert(meshlink_channel_send(mesh, channel, "reply", 5) >= 0);
		}
	}

	return;
}

// Meshlink log handler
static void meshlink_logger(meshlink_handle_t *mesh, meshlink_log_level_t level,
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

	bool mtu_probe = false;

	if(node_shutdown_time && cur_time >= node_shutdown_time) {
		test_pmtu_nut_running = false;
	}

	if(level == MESHLINK_INFO) {
		fprintf(stderr, "\x1b[32m nut:\x1b[0m %s\n", text);
	}

	/* Calculate the MTU parameter values from the meshlink logs */
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

		if(!node_shutdown_time && !strcasecmp("peer", node_name) && mtu_set) {
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

	} else if((sscanf(text, "Connection closed by %s", node_name) == 1) || (sscanf(text, "Closing connection with %s", node_name) == 1)) {
	}
}

void *node_sim_pmtu_nut_01(void *arg) {
	mesh_arg_t *mesh_arg = (mesh_arg_t *)arg;
	struct timeval main_loop_wait = { 5, 0 };

	set_sync_flag(&peer_reachable, false);
	set_sync_flag(&channel_opened, false);
	node_shutdown_time = 0;
	mtu_set = true;

	// Run relay node instance

	meshlink_handle_t *mesh;
	mesh = meshlink_open(mesh_arg->node_name, mesh_arg->confbase, mesh_arg->app_name, mesh_arg->dev_class);
	assert(mesh);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, meshlink_logger);
	meshlink_set_node_status_cb(mesh, node_status_cb);
	sleep(1);

	// Join relay node and if fails to join then try few more attempts

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
			fail();
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

	// All test steps executed - wait for signals to stop/start or close the mesh

	time_t time_stamp, send_time;

	time_stamp = time(NULL);
	send_time = time_stamp + 10;

	while(test_pmtu_nut_running) {
		select(1, NULL, NULL, NULL, &main_loop_wait);

		// Ping the channel for every 10 seconds if ping_channel_enable_07 is enabled

		if(ping_channel_enable_07) {
			time_stamp = time(NULL);

			if(time_stamp >= send_time) {
				send_time = time_stamp + 10;
				meshlink_channel_send(mesh, channel, "ping", 5);
			}
		}
	}

	// Send MTU probe parameters data to the test driver
	meshlink_close(mesh);

	set_sync_flag(&test_pmtu_nut_closed, true);
	fprintf(stderr, "NODE_PMTU_PEER :\n");
	print_mtu_calc(node_pmtu[NODE_PMTU_PEER]);
	fprintf(stderr, "\nNODE_PMTU_RELAY :\n");
	print_mtu_calc(node_pmtu[NODE_PMTU_RELAY]);
}
