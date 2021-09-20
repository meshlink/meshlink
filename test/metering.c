#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "meshlink.h"
#include "devtools.h"
#include "netns_utils.h"
#include "utils.h"

static struct sync_flag peer_reachable;
static struct sync_flag aio_done;
static const size_t size = 10000000;
static char *buffer;

static void nut_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(reachable && !strcmp(node->name, "peer")) {
		set_sync_flag(&peer_reachable, true);
	}
}

static void aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;
	(void)priv;

	set_sync_flag(&aio_done, true);
}

static void peer_aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;
	(void)priv;

	meshlink_channel_close(mesh, channel);
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)port;
	(void)data;
	(void)len;

	assert(buffer);
	assert(meshlink_channel_aio_receive(mesh, channel, buffer, size, peer_aio_cb, NULL));
	return true;
}

static void print_counters(peer_config_t *peers, const char *description) {
	printf("%s:\n", description);
	printf("        %9s %9s %9s %9s %9s %9s\n",
	       "in data",
	       "forward",
	       "meta",
	       "out data",
	       "forward",
	       "meta");

	for(int i = 0; i < 3; i++) {
		meshlink_node_t *node = meshlink_get_node(peers[0].mesh, peers[i].name);
		assert(node);
		struct devtool_node_status status;
		devtool_reset_node_counters(peers[0].mesh, node, &status);
		printf(" %5s: %9" PRIu64 " %9" PRIu64 " %9" PRIu64 " %9" PRIu64 " %9" PRIu64 " %9" PRIu64 "\n",
		       node->name,
		       status.in_data,
		       status.in_forward,
		       status.in_meta,
		       status.out_data,
		       status.out_forward,
		       status.out_meta);
	}
}


int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	init_sync_flag(&peer_reachable);
	init_sync_flag(&aio_done);

	// Set up relay, peer and NUT
	peer_config_t *peers = setup_relay_peer_nut("metering");

	meshlink_set_node_status_cb(peers[2].mesh, nut_status_cb);

	for(int i = 0; i < 3; i++) {
		assert(meshlink_start(peers[i].mesh));
	}

	// Measure traffic after 1 minute of PMTU probing
	sleep(60);
	print_counters(peers, "PMTU probing (1 min)");

	// Measure traffic after 1 minute of idle
	for(int i = 0; i < 10; i++) {
		sleep(60);
		print_counters(peers, "Idle (1 min)");
	}

	// Measure channel traffic between relay and peer
	buffer = calloc(1, size);
	assert(buffer);
	meshlink_node_t *peer = meshlink_get_node(peers[0].mesh, peers[1].name);
	assert(peer);
	meshlink_set_channel_accept_cb(peers[1].mesh, accept_cb);
	meshlink_channel_t *channel = meshlink_channel_open(peers[0].mesh, peer, 1, NULL, NULL, 0);
	assert(channel);
	assert(meshlink_channel_aio_send(peers[0].mesh, channel, buffer, size, aio_cb, NULL));
	assert(wait_sync_flag(&aio_done, 15));
	meshlink_channel_close(peers[0].mesh, channel);
	sleep(1);
	print_counters(peers, "relay->peer channel traffic");

	// Measure channel traffic between NUT and peer
	assert(wait_sync_flag(&peer_reachable, 5));
	meshlink_node_t *nut = meshlink_get_node(peers[0].mesh, peers[2].name);
	assert(nut);
	peer = meshlink_get_node(peers[2].mesh, peers[1].name);
	assert(peer);
	channel = meshlink_channel_open(peers[2].mesh, peer, 1, NULL, NULL, 0);
	assert(channel);
	init_sync_flag(&aio_done);
	assert(meshlink_channel_aio_send(peers[2].mesh, channel, buffer, size, aio_cb, NULL));
	assert(wait_sync_flag(&aio_done, 15));
	meshlink_channel_close(peers[2].mesh, channel);
	sleep(1);
	print_counters(peers, "NUT->peer channel traffic");

	close_relay_peer_nut(peers);
}
