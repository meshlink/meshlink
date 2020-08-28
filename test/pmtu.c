#ifndef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "meshlink.h"
#include "devtools.h"
#include "utils.h"

#define nnodes 3

static const struct info {
	const char *name;
	const char *confdir;
	const char *netns;
	dev_class_t devclass;
} nodes[nnodes] = {
	{"relay", "pmtu_conf.1", "/run/netns/pmtu_r", DEV_CLASS_BACKBONE},
	{"peer", "pmtu_conf.2", "/run/netns/pmtu_p", DEV_CLASS_STATIONARY},
	{"nut", "pmtu_conf.3", "/run/netns/pmtu_n", DEV_CLASS_STATIONARY},
};

static struct state {
	meshlink_handle_t *mesh;
	int netns;
	struct sync_flag up_flag;
	int pmtu;
	int probe_count;
	int probe_bytes;
} states[nnodes];

static void relay_up_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	struct state *state = mesh->priv;

	// Check that we are connected to another peer besides the relay
	if(reachable && node != meshlink_get_self(mesh) && strcmp(node->name, "relay")) {
		set_sync_flag(&state->up_flag, true);
		meshlink_set_node_status_cb(mesh, NULL);
	}
}

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	if(!data && !len) {
		meshlink_channel_close(mesh, channel);
	}
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)port;
	(void)data;
	(void)len;
	meshlink_set_channel_receive_cb(mesh, channel, receive_cb);
	return true;
}

static void wait_for_pmtu(void) {
	// Set up a channel from peer to nut
	meshlink_set_channel_accept_cb(states[2].mesh, accept_cb);
	meshlink_handle_t *mesh = states[1].mesh;
	meshlink_node_t *peer = meshlink_get_node(mesh, nodes[2].name);
	meshlink_channel_t *channel = meshlink_channel_open(mesh, peer, 1, NULL, NULL, 0);
	assert(channel);

	// While sending regular data, wait for PMTU discovery to finish
	for(int i = 0; i < 30; i++) {
		sleep(1);

		devtool_node_status_t status;
		devtool_get_node_status(mesh, peer, &status);
		states[1].pmtu = status.minmtu;

		if(status.minmtu == status.maxmtu) {
			break;
		}

		assert(meshlink_channel_send(mesh, channel, "ping", 4) == 4);
	}

	meshlink_channel_close(mesh, channel);
}

static void wait_for_udp_timeout(void) {
	// Set up a channel from peer to nut
	meshlink_set_channel_accept_cb(states[2].mesh, accept_cb);
	meshlink_handle_t *mesh = states[1].mesh;
	meshlink_node_t *peer = meshlink_get_node(mesh, nodes[2].name);
	meshlink_channel_t *channel = meshlink_channel_open(mesh, peer, 1, NULL, NULL, 0);
	assert(channel);

	// While sending regular data, wait for UDP to time out
	for(int i = 0; i < 20; i++) {
		sleep(1);

		devtool_node_status_t status;
		devtool_get_node_status(mesh, peer, &status);
		states[1].pmtu = status.minmtu;

		if(!status.minmtu) {
			break;
		}

		assert(meshlink_channel_send(mesh, channel, "ping", 4) == 4);
	}

	meshlink_channel_close(mesh, channel);
}

static void start_peer_nut(void) {
	// Start peer and nut
	for(int i = 1; i < nnodes; i++) {
		meshlink_set_node_status_cb(states[i].mesh, relay_up_cb);
		assert(meshlink_start(states[i].mesh));
	}

	// Wait for the peer and nut to see each other
	for(int i = 1; i < nnodes; i++) {
		assert(wait_sync_flag(&states[i].up_flag, 5));
	}
}

static void stop_peer_nut(void) {
	// Stop peer and nut, reset counters
	for(int i = 1; i < nnodes; i++) {
		meshlink_stop(states[i].mesh);
		states[i].up_flag.flag = false;
		states[i].pmtu = 0;
		states[i].probe_count = 0;
		states[i].probe_bytes = 0;
	}
}

int main(void) {
	// This test requires root access due to the use of network namespaces
	if(getuid() != 0) {
		return 77;
	}

	// Set up namespaces
	assert(system("./pmtu-setup") == 0);

	// Bring up the nodes
	for(int i = 0; i < nnodes; i++) {
		assert(meshlink_destroy(nodes[i].confdir));

		// Open the network namespace
		states[i].netns = open(nodes[i].netns, O_RDONLY);
		assert(states[i].netns != -1);

		// Open the MeshLink instance
		meshlink_open_params_t *params;
		assert(params = meshlink_open_params_init(nodes[i].confdir, nodes[i].name, "pmtu", nodes[i].devclass));
		assert(meshlink_open_params_set_netns(params, states[i].netns));
		assert(states[i].mesh = meshlink_open_ex(params));
		free(params);

		states[i].mesh->priv = &states[i];
		meshlink_enable_discovery(states[i].mesh, false);
		init_sync_flag(&states[i].up_flag);

		meshlink_set_log_cb(states[i].mesh, MESHLINK_INFO, log_cb);

		// Link the relay node to the other nodes
		if(i > 0) {
			link_meshlink_pair(states[0].mesh, states[i].mesh);
		}
	}

	// Start the relay
	assert(meshlink_start(states[0].mesh));

	// Start peers and wait for them to connect
	start_peer_nut();

	// Wait for PMTU discovery to finish
	wait_for_pmtu();

	assert(states[1].pmtu >= 1400 && states[1].pmtu <= 1500);
	assert(states[1].probe_count <= 10);
	assert(states[1].probe_bytes <= 1500 * 10);

	// Drop the MTU to 800
	stop_peer_nut();

	assert(system("ip netns exec pmtu_p ip link set eth0 mtu 800") == 0);
	assert(system("ip netns exec pmtu_n ip link set eth0 mtu 800") == 0);

	// Workaround for autoconnect algorithm throttling reconnects
	sleep(15);

	start_peer_nut();
	wait_for_pmtu();

	assert(states[1].pmtu >= 700 && states[1].pmtu <= 800);
	assert(states[1].probe_count <= 20);
	assert(states[1].probe_bytes <= 800 * 20);

	// Block UDP

	assert(system("ip netns exec pmtu_p iptables -A INPUT -p udp -j DROP") == 0);
	assert(system("ip netns exec pmtu_n iptables -A INPUT -p udp -j DROP") == 0);

	// Wait

	wait_for_udp_timeout();
	assert(states[1].pmtu == 0);

	// Cleanup
	for(int i = 0; i < nnodes; i++) {
		meshlink_close(states[i].mesh);
	}
}
