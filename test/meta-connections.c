#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "meshlink.h"
#include "../src/devtools.h"
#include "netns_utils.h"
#include "utils.h"

static struct sync_flag peer_reachable;
static struct sync_flag peer_unreachable;

static void nut_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!strcmp(node->name, "peer")) {
		set_sync_flag(reachable ? &peer_reachable : &peer_unreachable, true);
	}
}

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	init_sync_flag(&peer_reachable);
	init_sync_flag(&peer_unreachable);

	// Set up relay, peer and NUT
	peer_config_t *peers = setup_relay_peer_nut("metaconn");

	// Wait for peer to connect to NUT
	devtool_set_meta_status_cb(peers[2].mesh, nut_status_cb);

	for(int i = 0; i < 3; i++) {
		assert(meshlink_start(peers[i].mesh));
	}

	assert(wait_sync_flag(&peer_reachable, 5));

	// Test case #1: re-connection to peer after disconnection when connected to the relay node

	// Stop the peer and wait for it to become unreachable
	reset_sync_flag(&peer_unreachable);
	meshlink_stop(peers[1].mesh);
	assert(wait_sync_flag(&peer_unreachable, 5));

	// Restart the peer and wait for it to become reachable
	reset_sync_flag(&peer_reachable);
	assert(meshlink_start(peers[1].mesh));
	assert(wait_sync_flag(&peer_reachable, 5));

	// Test case #2: re-connection to peer after changing peer and NUT's IP address simultaneously,
	//               while connected to the relay

	reset_sync_flag(&peer_reachable);
	reset_sync_flag(&peer_unreachable);

	for(int i = 1; i < 3; i++) {
		change_peer_ip(&peers[i]);
	}

	for(int i = 1; i < 3; i++) {
		meshlink_reset_timers(peers[i].mesh);
	}

	assert(wait_sync_flag(&peer_unreachable, 75));
	assert(wait_sync_flag(&peer_reachable, 15));

	// Test case #3: re-connect to peer after stopping NUT and changing peer's IP address, no relay
	reset_sync_flag(&peer_unreachable);

	for(int i = 0; i < 2; i++) {
		meshlink_stop(peers[i].mesh);
	}

	change_peer_ip(&peers[1]);
	assert(wait_sync_flag(&peer_unreachable, 15));

	reset_sync_flag(&peer_reachable);
	assert(meshlink_start(peers[1].mesh));
	assert(wait_sync_flag(&peer_reachable, 60));

	// Done.

	close_relay_peer_nut(peers);
}
