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
static struct sync_flag aio_finished;

static const size_t size = 25000000; // size of data to transfer

static void a_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		set_sync_flag(&b_responded, true);
	}
}

static void b_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	assert(meshlink_channel_send(mesh, channel, data, len) == (ssize_t)len);
}

static bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;

	return false;
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
	meshlink_set_channel_sndbuf(mesh, channel, size);

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

static void aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;
	(void)priv;

	set_sync_flag(&aio_finished, true);
}

int main(void) {
	init_sync_flag(&b_responded);
	init_sync_flag(&aio_finished);

	meshlink_set_log_cb(NULL, MESHLINK_INFO, log_cb);

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels-buffer-storage");

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh_a, reject_cb);
	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	// Start both instances

	start_meshlink_pair(mesh_a, mesh_b);

	// Open a channel from a to b.

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channel = meshlink_channel_open(mesh_a, b, 7, a_receive_cb, NULL, 0);
	assert(channel);

	size_t buf_size = 1024 * 1024;
	char *sndbuf = malloc(1024 * 1024);
	assert(sndbuf);
	char *rcvbuf = malloc(1024 * 1024);
	assert(rcvbuf);

	// Set external buffers

	meshlink_set_channel_sndbuf_storage(mesh_a, channel, sndbuf, buf_size);
	meshlink_set_channel_rcvbuf_storage(mesh_a, channel, rcvbuf, buf_size);

	// Check that we can transition back and forth to external buffers

	meshlink_set_channel_sndbuf_storage(mesh_a, channel, NULL, 4096);
	meshlink_set_channel_rcvbuf(mesh_a, channel, 4096);

	meshlink_set_channel_sndbuf_storage(mesh_a, channel, sndbuf, buf_size);
	meshlink_set_channel_rcvbuf_storage(mesh_a, channel, rcvbuf, buf_size);

	// Wait for the channel to finish connecting

	meshlink_set_channel_poll_cb(mesh_a, channel, poll_cb);
	assert(wait_sync_flag(&b_responded, 20));

	// Send a lot of data

	char *outdata = malloc(size);
	assert(outdata);

	for(size_t i = 0; i < size; i++) {
		outdata[i] = i;
	}

	char *indata = malloc(size);
	assert(indata);

	assert(meshlink_channel_aio_receive(mesh_a, channel, indata, size, aio_cb, NULL));
	assert(meshlink_channel_aio_send(mesh_a, channel, outdata, size, NULL, NULL));
	assert(wait_sync_flag(&aio_finished, 20));
	assert(!memcmp(indata, outdata, size));

	// Done

	meshlink_channel_close(mesh_a, channel);

	// Clean up.

	free(indata);
	free(outdata);
	free(rcvbuf);
	free(sndbuf);

	close_meshlink_pair(mesh_a, mesh_b);
}
