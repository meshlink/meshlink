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

#define SMALL_SIZE 512
#define SMALL_COUNT 2500
#define LARGE_SIZE 131072
#define NCHANNELS 10
#define PORT 123

static struct channel {
	meshlink_channel_t *channel;
	bool udp;
	struct sync_flag open_flag;
	struct sync_flag close_flag;
} channels[NCHANNELS];

static size_t received[NCHANNELS];

static struct sync_flag pmtu_flag;

static void a_pmtu_cb(meshlink_handle_t *mesh, meshlink_node_t *node, uint16_t pmtu) {
	assert(mesh);

	if(pmtu >= SMALL_SIZE && !strcmp(node->name, "b")) {
		set_sync_flag(&pmtu_flag, true);
	}
}

static void a_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	struct channel *info = channel->priv;
	assert(info);

	if(!data && !len) {
		set_sync_flag(&info->close_flag, true);
		meshlink_channel_close(mesh, channel);
		return;
	}
}

static void aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	(void)data;
	(void)len;
	(void)priv;

	meshlink_channel_shutdown(mesh, channel, SHUT_WR);
}

static void b_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	if(!data && !len) {
		meshlink_channel_close(mesh, channel);
		return;
	}

	assert(data && len);
	size_t *rx = channel->priv;
	*rx += len;
}

static bool b_accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)data;
	(void)len;

	assert(!data);
	assert(!len);
	assert(port == PORT);

	static int i;
	channel->priv = &received[i++];

	meshlink_set_channel_receive_cb(mesh, channel, b_receive_cb);
	return true;
}

static void a_poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	assert(len);

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	struct channel *info = channel->priv;
	assert(info);

	set_sync_flag(&info->open_flag, true);
}

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	meshlink_handle_t *a, *b;
	open_meshlink_pair(&a, &b, "channels-mixed");

	meshlink_set_channel_accept_cb(b, b_accept_cb);
	meshlink_set_node_pmtu_cb(a, a_pmtu_cb);
	start_meshlink_pair(a, b);

	// Create a number of TCP and UDP channels

	meshlink_node_t *nb = meshlink_get_node(a, "b");

	for(int i = 0; i < NCHANNELS; i++) {
		channels[i].udp = i % 2 == 1;
		init_sync_flag(&channels[i].open_flag);
		init_sync_flag(&channels[i].close_flag);
		channels[i].channel = meshlink_channel_open_ex(a, nb, PORT, a_receive_cb, &channels[i], 0, channels[i].udp ? MESHLINK_CHANNEL_UDP : MESHLINK_CHANNEL_TCP);
		meshlink_set_channel_poll_cb(a, channels[i].channel, a_poll_cb);
		assert(channels[i].channel);
	}

	// Wait for all channels to connect

	for(int i = 0; i < NCHANNELS; i++) {
		assert(wait_sync_flag(&channels[i].open_flag, 10));
	}

	// Wait for PMTU to finish

	assert(wait_sync_flag(&pmtu_flag, 10));

	// Send data on all channels

	size_t size = SMALL_SIZE * SMALL_COUNT;
	char *data = malloc(size);
	memset(data, 'U', size);

	for(int i = 0; i < NCHANNELS; i++) {
		assert(meshlink_channel_aio_send(a, channels[i].channel, data, size, aio_cb, &channels[i]));
	}

	// Wait for the other end to close the channels

	for(int i = 0; i < NCHANNELS; i++) {
		assert(wait_sync_flag(&channels[i].close_flag, 10));
	}

	// Check that most of the data has been transmitted

	for(int i = 0; i < NCHANNELS; i++) {
		fprintf(stderr, "%d received %zu\n", i, received[i]);
	}

	int received_all = 0;

	for(int i = 0; i < NCHANNELS; i++) {
		assert(received[i] >= size / 2);
		assert(received[i] <= size);

		if(received[i] == size) {
			received_all++;
		}
	}

	assert(received_all >= NCHANNELS / 2);

	// Done

	close_meshlink_pair(a, b);

	return 0;
}
