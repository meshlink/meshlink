#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "utils.h"
#include "../src/meshlink.h"

static struct sync_flag b_responded;

static void a_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;

	printf("a_receive_cb %zu: ", len);
	fwrite(data, 1, len, stdout);
	printf("\n");

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		set_sync_flag(&b_responded, true);
	}
}

static void b_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	printf("b_receive_cb %zu: ", len);
	fwrite(data, 1, len, stdout);
	printf("\n");
	// Echo the data back.
	assert(meshlink_channel_send(mesh, channel, data, len) == (ssize_t)len);
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	printf("accept_cb: (from %s on port %u) ", channel->node->name, (unsigned int)port);

	if(data) {
		fwrite(data, 1, len, stdout);
	}

	printf("\n");

	if(port != 7) {
		return false;
	}

	meshlink_set_channel_receive_cb(mesh, channel, b_receive_cb);

	if(data) {
		b_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	assert(meshlink_channel_send(mesh, channel, "Hello", 5) == 5);
}

int main(void) {
	init_sync_flag(&b_responded);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels");

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	// Start both instances

	start_meshlink_pair(mesh_a, mesh_b);

	// Open a channel from a to b.

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channel = meshlink_channel_open(mesh_a, b, 7, a_receive_cb, NULL, 0);
	assert(channel);

	meshlink_set_channel_poll_cb(mesh_a, channel, poll_cb);
	assert(wait_sync_flag(&b_responded, 20));

	meshlink_channel_abort(mesh_a, channel);

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
}
