#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "../src/meshlink.h"
#include "utils.h"

static size_t received;
static struct sync_flag accept_flag;
static struct sync_flag small_flag;
static struct sync_flag large_flag;
static struct sync_flag close_flag;

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;

	if(!data && !len) {
		meshlink_channel_close(mesh, channel);
		set_sync_flag(&close_flag, true);
	}

	if(len >= 2) {
		uint16_t checklen;
		memcpy(&checklen, data, sizeof(checklen));
		assert(len == checklen);
	}

	if(len == 65535) {
		set_sync_flag(&large_flag, true);
	}

	if(len == 0) {
		set_sync_flag(&small_flag, true);
	}

	received += len;
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	assert(port == 1);
	assert(!data);
	assert(!len);
	assert(meshlink_channel_get_flags(mesh, channel) == (MESHLINK_CHANNEL_TCP | MESHLINK_CHANNEL_FRAMED));
	meshlink_set_channel_receive_cb(mesh, channel, receive_cb);
	set_sync_flag(&accept_flag, true);

	return true;
}

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	// Open two meshlink instances

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels_framed");
	start_meshlink_pair(mesh_a, mesh_b);

	// Create a channel from a to b

	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_a, b, 1, NULL, NULL, 0, MESHLINK_CHANNEL_TCP | MESHLINK_CHANNEL_FRAMED);
	assert(channel);

	size_t sndbuf_size = 128 * 1024;
	meshlink_set_channel_sndbuf(mesh_a, channel, sndbuf_size);

	// Check that we can send zero bytes

	assert(meshlink_channel_send(mesh_a, channel, "", 0) == 0);
	assert(wait_sync_flag(&small_flag, 1));

	// Check that we cannot send more than 65535 bytes without errors

	char data[65535] = "";
	assert(meshlink_channel_send(mesh_a, channel, data, 65536) == -1);

	// Check that we can send 65535 bytes

	uint16_t framelen = 65535;
	memcpy(data, &framelen, sizeof(framelen));
	assert(meshlink_channel_send(mesh_a, channel, data, framelen) == framelen);
	assert(wait_sync_flag(&large_flag, 1));

	// Send randomly sized frames from a to b

	size_t total_len = framelen;

	for(int j = 0; j < 2500; j++) {
		framelen = rand() % 2048;
		memcpy(data, &framelen, sizeof(framelen));

		while(meshlink_channel_get_sendq(mesh_a, channel) > sndbuf_size - framelen - 2) {
			const struct timespec req = {0, 2000000};
			clock_nanosleep(CLOCK_MONOTONIC, 0, &req, NULL);
		}

		assert(meshlink_channel_send(mesh_a, channel, data, framelen) == framelen);
		total_len += framelen;
	}

	// Closes the channel and wait for the other end to closes it as well

	meshlink_channel_close(mesh_a, channel);
	assert(wait_sync_flag(&close_flag, 10));

	// Check that the clients have received all the data we sent

	assert(received == total_len);

	close_meshlink_pair(mesh_a, mesh_b);

	return 0;
}
