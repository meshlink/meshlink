#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "../src/meshlink.h"
#include "utils.h"

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;

	return true;
}

static struct sync_flag poll_flag;
static size_t poll_len;

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	poll_len = len;
	set_sync_flag(&poll_flag, true);
}

static struct sync_flag receive_flag;
static size_t receive_len;

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)data;

	receive_len = len;
	set_sync_flag(&receive_flag, true);
}

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two meshlink instances.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels_failure");

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	// Open a channel from a to b

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channel = meshlink_channel_open(mesh_a, b, 7, receive_cb, NULL, 0);
	assert(channel);

	meshlink_set_channel_poll_cb(mesh_a, channel, poll_cb);

	// Start both instances

	start_meshlink_pair(mesh_a, mesh_b);

	// Wait for the channel to be established

	assert(wait_sync_flag(&poll_flag, 10));
	assert(poll_len != 0);

	sleep(1);

	// Set a very small timeout for channels to b.

	meshlink_set_node_channel_timeout(mesh_a, b, 1);

	// Stop mesh_b. We should get a notification that the channel has closed after a while.

	meshlink_stop(mesh_b);

	assert(wait_sync_flag(&receive_flag, 5));
	assert(receive_len == 0);

	meshlink_channel_close(mesh_a, channel);

	// Try setting up a new channel while b is still down.

	poll_flag.flag = false;
	receive_flag.flag = false;

	channel = meshlink_channel_open(mesh_a, b, 7, NULL, NULL, 0);
	assert(channel);

	meshlink_set_channel_poll_cb(mesh_a, channel, poll_cb);

	assert(wait_sync_flag(&poll_flag, 5));
	assert(poll_len == 0);

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
}
