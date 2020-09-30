#define _GNU_SOURCE

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <pthread.h>

#include "../src/meshlink.h"
#include "utils.h"

static struct sync_flag b_responded;
static struct sync_flag b_closed;
static size_t a_poll_cb_len;

static void a_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		set_sync_flag(&b_responded, true);
	} else if(len == 0) {
		set_sync_flag(&b_closed, true);
		set_sync_flag(channel->priv, true);
	}
}

static void b_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	// Send one message back, then close the channel.
	if(len) {
		assert(meshlink_channel_send(mesh, channel, data, len) == (ssize_t)len);
	}

	meshlink_channel_close(mesh, channel);
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)port;

	meshlink_set_channel_accept_cb(mesh, NULL);
	meshlink_set_channel_receive_cb(mesh, channel, b_receive_cb);

	if(data) {
		b_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	set_sync_flag(channel->priv, true);
}

static void poll_cb2(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)mesh;
	(void)channel;

	a_poll_cb_len = len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	set_sync_flag(channel->priv, true);
}

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	init_sync_flag(&b_responded);
	init_sync_flag(&b_closed);

	meshlink_handle_t *a, *b;
	open_meshlink_pair(&a, &b, "channels-udp-cornercases");

	// Set the callbacks.

	meshlink_set_channel_accept_cb(b, accept_cb);

	// Open a channel from a to b before starting the mesh.

	meshlink_node_t *nb = meshlink_get_node(a, "b");
	assert(nb);

	struct sync_flag channel_opened;
	init_sync_flag(&channel_opened);

	meshlink_channel_t *channel = meshlink_channel_open_ex(a, nb, 7, a_receive_cb, &channel_opened, 0, MESHLINK_CHANNEL_UDP);
	assert(channel);

	meshlink_set_channel_poll_cb(a, channel, poll_cb);

	// Check that the channel isn't established yet and sending a packet at this point returns 0
	assert(meshlink_channel_send(a, channel, "test", 4) == 0);
	assert(wait_sync_flag(&channel_opened, 1) == false);

	// Start MeshLink and wait for the channel to become connected.
	start_meshlink_pair(a, b);

	assert(wait_sync_flag(&channel_opened, 15));

	// Re-initialize everything
	meshlink_channel_close(a, channel);
	close_meshlink_pair(a, b);
	reset_sync_flag(&channel_opened);
	reset_sync_flag(&b_responded);
	reset_sync_flag(&b_closed);
	open_meshlink_pair(&a, &b, "channels-udp-cornercases");

	meshlink_set_channel_accept_cb(b, accept_cb);

	start_meshlink_pair(a, b);

	// Create a channel to b
	nb = meshlink_get_node(a, "b");
	assert(nb);

	channel = meshlink_channel_open_ex(a, nb, 7, a_receive_cb, &channel_opened, 0, MESHLINK_CHANNEL_UDP);
	assert(channel);
	meshlink_set_channel_poll_cb(a, channel, poll_cb);

	assert(wait_sync_flag(&channel_opened, 15));

	// Send a message to b

	struct sync_flag channel_closed;
	init_sync_flag(&channel_closed);
	channel->priv = &channel_closed;

	for(int i = 0; i < 10; i++) {
		assert(meshlink_channel_send(a, channel, "Hello", 5) == 5);

		if(wait_sync_flag(&channel_closed, 1)) {
			break;
		}
	}

	assert(wait_sync_flag(&channel_closed, 1));

	wait_sync_flag(&b_responded, 1);
	wait_sync_flag(&b_closed, 1);

	// Try to send data on a closed channel

	for(int i = 0; i < 10; i++) {
		if(meshlink_channel_send(a, channel, "Hello", 5) == -1) {
			break;
		}

		assert(i != 9);
		usleep(10000);
	}

	// Try to create a second channel

	struct sync_flag channel_polled;
	init_sync_flag(&channel_polled);

	meshlink_channel_t *channel2 = meshlink_channel_open_ex(a, nb, 7, a_receive_cb, &channel_polled, 0, MESHLINK_CHANNEL_UDP);
	assert(channel2);
	meshlink_set_channel_poll_cb(a, channel2, poll_cb2);

	assert(wait_sync_flag(&channel_polled, 5));

	assert(0 == a_poll_cb_len);

	meshlink_channel_close(a, channel);
	meshlink_channel_close(a, channel2);
	close_meshlink_pair(a, b);
}
