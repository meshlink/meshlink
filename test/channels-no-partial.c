#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "meshlink.h"
#include "utils.h"

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;

	return true;
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Start two new meshlink instance.

	meshlink_handle_t *mesh_a;
	meshlink_handle_t *mesh_b;

	open_meshlink_pair(&mesh_a, &mesh_b, "channels_no_partial");
	meshlink_set_channel_accept_cb(mesh_b, accept_cb);
	start_meshlink_pair(mesh_a, mesh_b);

	// Open a channel

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_a, b, 1, NULL, NULL, 0, MESHLINK_CHANNEL_TCP | MESHLINK_CHANNEL_NO_PARTIAL);
	assert(channel);

	// Stop a to ensure we get deterministic behaviour for the channel send queue.

	meshlink_stop(mesh_a);

	// Verify that no partial sends succeed.
	// If rejected sends would fit an empty send buffer, 0 should be returned, otherwise -1.

	char buf[512] = "data";

	meshlink_set_channel_sndbuf(mesh_a, channel, 256);
	assert(meshlink_channel_send(mesh_a, channel, buf, 257) == -1);
	assert(meshlink_channel_send(mesh_a, channel, buf, 256) == 256);

	meshlink_set_channel_sndbuf(mesh_a, channel, 512);
	assert(meshlink_channel_send(mesh_a, channel, buf, 257) == 0);
	assert(meshlink_channel_send(mesh_a, channel, buf, 128) == 128);
	assert(meshlink_channel_send(mesh_a, channel, buf, 129) == 0);
	assert(meshlink_channel_send(mesh_a, channel, buf, 100) == 100);
	assert(meshlink_channel_send(mesh_a, channel, buf, 29) == 0);
	assert(meshlink_channel_send(mesh_a, channel, buf, 513) == -1);

	// Restart a to ensure it gets to flush the channel send queue.

	meshlink_start(mesh_a);

	assert_after(!meshlink_channel_get_sendq(mesh_a, channel), 30);
	assert(meshlink_channel_send(mesh_a, channel, buf, 512) == 512);

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
}
