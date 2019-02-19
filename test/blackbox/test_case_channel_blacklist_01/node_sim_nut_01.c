/*
    node_sim_nut.c -- Implementation of Node Simulation for Meshlink Testing
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
#include "../common/mesh_event_handler.h"
#include "../common/network_namespace_framework.h"
#include "../../utils.h"
#include "node_sim_nut_01.h"

#define CHANNEL_PORT 1234

static bool blacklist_set;
int total_reachable_callbacks_01;
int total_unreachable_callbacks_01;
int total_channel_closure_callbacks_01;
bool channel_discon_case_ping;
bool channel_discon_network_failure_01;
bool channel_discon_network_failure_02;
bool test_blacklist_whitelist_01;
bool test_channel_restart_01;

static struct sync_flag peer_reachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag peer_unreachable = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag channel_opened = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};
static struct sync_flag channels_closed = {.mutex  = PTHREAD_MUTEX_INITIALIZER, .cond = PTHREAD_COND_INITIALIZER};

static void node_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node,
                           bool reachable) {

	fprintf(stderr, "Node %s %s\n", node->name, reachable ? "reachable" : "unreachable");

	if(!strcmp(node->name, "peer")) {
		if(reachable) {
			set_sync_flag(&peer_reachable, true);

			if(blacklist_set) {
				++total_reachable_callbacks_01;
			}
		} else {
			set_sync_flag(&peer_unreachable, true);

			if(blacklist_set) {
				++total_unreachable_callbacks_01;
			}
		}
	}

	return;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
	fprintf(stderr, "%s poll cb invoked\n", (char *)channel->priv);
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	assert(meshlink_channel_send(mesh, channel, "test", 5) >= 0);
	return;
}

static void channel_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
	if(len == 0) {
		fprintf(stderr, "Closed channel with %s\n", (char *)channel->priv);

		if(blacklist_set) {
			++total_channel_closure_callbacks_01;
		}

		if(total_channel_closure_callbacks_01 == 2) {
			set_sync_flag(&channels_closed, true);
		}
	}

	if(!strcmp(channel->node->name, "peer")) {
		if(len == 5 && !memcmp(dat, "reply", 5)) {
			fprintf(stderr, "Channel opened with %s\n", (char *)channel->priv);
			set_sync_flag(&channel_opened, true);
		}
	}

	return;
}

static void log_message(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)mesh;

	fprintf(stderr, "\x1b[32m nut:\x1b[0m %s\n", text);
}

void *test_channel_blacklist_disonnection_nut_01(void *arg) {
	mesh_arg_t *mesh_arg = (mesh_arg_t *)arg;
	total_reachable_callbacks_01 = 0;
	total_unreachable_callbacks_01 = 0;
	total_channel_closure_callbacks_01 = 0;

	set_sync_flag(&peer_reachable, false);
	set_sync_flag(&peer_unreachable, false);
	set_sync_flag(&channel_opened, false);
	blacklist_set = false;

	assert(!channel_discon_network_failure_01 || !channel_discon_network_failure_02);

	// Run relay node instance

	meshlink_handle_t *mesh;
	mesh = meshlink_open(mesh_arg->node_name, mesh_arg->confbase, mesh_arg->app_name, mesh_arg->dev_class);
	assert(mesh);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_message);
	meshlink_set_node_status_cb(mesh, node_status_cb);

	// Join relay node and if fails to join then try few more attempts

	if(mesh_arg->join_invitation) {
		assert(meshlink_join(mesh, mesh_arg->join_invitation));
	}

	assert(meshlink_start(mesh));

	// Wait for peer node to join

	assert(wait_sync_flag(&peer_reachable, 30));

	meshlink_node_t *peer_node = meshlink_get_node(mesh, "peer");
	assert(peer_node);
	meshlink_channel_t *channel1 = meshlink_channel_open(mesh, peer_node, CHANNEL_PORT,
	                               channel_receive_cb, NULL, 0);
	channel1->priv = "channel1";
	meshlink_set_channel_poll_cb(mesh, channel1, poll_cb);

	assert(wait_sync_flag(&channel_opened, 15));

	set_sync_flag(&channel_opened, false);

	meshlink_channel_t *channel2 = meshlink_channel_open(mesh, peer_node, CHANNEL_PORT,
	                               channel_receive_cb, NULL, 0);
	channel2->priv = "channel2";
	meshlink_set_channel_poll_cb(mesh, channel2, poll_cb);

	assert(wait_sync_flag(&channel_opened, 15));

	blacklist_set = true;

	if(channel_discon_network_failure_01) {
		fprintf(stderr, "Simulating network failure before blacklisting\n");
		assert(system("iptables -A INPUT -m statistic --mode random --probability 0.9 -j DROP") == 0);
		assert(system("iptables -A OUTPUT -m statistic --mode random --probability 0.9 -j DROP") == 0);
		sleep(1);
	}

	fprintf(stderr, "Node blacklisted\n");
	set_sync_flag(&channels_closed, false);
	meshlink_blacklist(mesh, peer_node);

	sleep(10);

	if(channel_discon_network_failure_02) {
		fprintf(stderr, "Simulating network failure after blacklisting\n");
		assert(system("iptables -A INPUT -m statistic --mode random --probability 0.9 -j DROP") == 0);
		assert(system("iptables -A OUTPUT -m statistic --mode random --probability 0.9 -j DROP") == 0);
		sleep(1);
	}

	if(channel_discon_case_ping) {
		fprintf(stderr, "Sending data through channels after blacklisting\n");
		assert(meshlink_channel_send(mesh, channel1, "ping", 5) >= 0);
		assert(meshlink_channel_send(mesh, channel2, "ping", 5) >= 0);
	}

	if(wait_sync_flag(&channels_closed, 120) == false) {
		set_sync_flag(&test_channel_discon_nut_close, true);
		return NULL;
	}

	if(channel_discon_network_failure_01 || channel_discon_network_failure_02) {
		fprintf(stderr, "Simulating network failure after blacklisting\n");
		assert(system("iptables -D INPUT -m statistic --mode random --probability 0.9 -j DROP") == 0);
		assert(system("iptables -D OUTPUT -m statistic --mode random --probability 0.9 -j DROP") == 0);
	}

	set_sync_flag(&peer_reachable, false);

	meshlink_whitelist(mesh, peer_node);
	fprintf(stderr, "Node whitelisted\n");

	wait_sync_flag(&peer_reachable, 70);

	fprintf(stderr, "Closing NUT instance\n");
	blacklist_set = false;

	set_sync_flag(&test_channel_discon_nut_close, true);

	meshlink_close(mesh);
	return NULL;
}
