#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "meshlink.h"
#include "utils.h"

static const size_t size = 25000000; // size of data to transfer
static const size_t smallsize = 100000; // size of the data to transfer without AIO
static const size_t nchannels = 4; // number of simultaneous channels

struct aio_info {
	int callbacks;
	size_t size;
	struct timespec ts;
	struct sync_flag flag;
};

struct channel_info {
	char *data;
	struct aio_info aio_infos[2];
};

static size_t b_received_len;
static struct timespec b_received_ts;
static struct sync_flag b_received_flag;

static void aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;

	struct aio_info *info = priv;
	clock_gettime(CLOCK_MONOTONIC, &info->ts);
	info->callbacks++;
	info->size += len;
	set_sync_flag(&info->flag, true);
}

static bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;

	return false;
}

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)data;

	b_received_len += len;

	if(b_received_len >= smallsize) {
		clock_gettime(CLOCK_MONOTONIC, &b_received_ts);
		set_sync_flag(&b_received_flag, true);
	}
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	assert(port && port <= nchannels + 1);
	assert(!data);
	assert(!len);

	if(port <= nchannels) {
		struct channel_info *infos = mesh->priv;
		struct channel_info *info = &infos[port - 1];

		assert(meshlink_channel_aio_receive(mesh, channel, info->data, size / 4, aio_cb, &info->aio_infos[0]));
		assert(meshlink_channel_aio_receive(mesh, channel, info->data + size / 4, size - size / 4, aio_cb, &info->aio_infos[1]));
	} else {
		meshlink_set_channel_receive_cb(mesh, channel, receive_cb);
	}

	return true;
}

int main(void) {
	init_sync_flag(&b_received_flag);

	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	// Prepare data buffers

	char *outdata = malloc(size);
	assert(outdata);

	for(size_t i = 0; i < size; i++) {
		outdata[i] = i;
	}

	struct channel_info in_infos[nchannels];

	struct channel_info out_infos[nchannels];

	memset(in_infos, 0, sizeof(in_infos));

	memset(out_infos, 0, sizeof(out_infos));

	for(size_t i = 0; i < nchannels; i++) {
		init_sync_flag(&in_infos[i].aio_infos[0].flag);
		init_sync_flag(&in_infos[i].aio_infos[1].flag);
		init_sync_flag(&out_infos[i].aio_infos[0].flag);
		init_sync_flag(&out_infos[i].aio_infos[1].flag);

		in_infos[i].data = malloc(size);
		assert(in_infos[i].data);
		out_infos[i].data = outdata;
	}

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels_aio");

	// Set the callbacks.

	mesh_b->priv = in_infos;

	meshlink_set_channel_accept_cb(mesh_a, reject_cb);
	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	// Start both instances

	start_meshlink_pair(mesh_a, mesh_b);

	// Open channels from a to b.

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channels[nchannels + 1];

	for(size_t i = 0; i < nchannels + 1; i++) {
		channels[i] = meshlink_channel_open(mesh_a, b, i + 1, NULL, NULL, 0);
		assert(channels[i]);
	}

	// Send a large buffer of data on each channel.

	for(size_t i = 0; i < nchannels; i++) {
		assert(meshlink_channel_aio_send(mesh_a, channels[i], outdata, size / 3, aio_cb, &out_infos[i].aio_infos[0]));
		assert(meshlink_channel_aio_send(mesh_a, channels[i], outdata + size / 3, size - size / 3, aio_cb, &out_infos[i].aio_infos[1]));
	}

	// Send a little bit on the last channel using a regular send

	assert(meshlink_channel_send(mesh_a, channels[nchannels], outdata, smallsize) == (ssize_t)smallsize);

	// Wait for everyone to finish.

	assert(wait_sync_flag(&b_received_flag, 10));

	for(size_t i = 0; i < nchannels; i++) {
		assert(wait_sync_flag(&out_infos[i].aio_infos[0].flag, 10));
		assert(wait_sync_flag(&out_infos[i].aio_infos[1].flag, 10));
		assert(wait_sync_flag(&in_infos[i].aio_infos[0].flag, 10));
		assert(wait_sync_flag(&in_infos[i].aio_infos[1].flag, 10));
	}

	// Check that everything is correct.

	assert(b_received_len == smallsize);

	for(size_t i = 0; i < nchannels; i++) {
		// Data should be transferred intact.
		assert(!memcmp(in_infos[i].data, out_infos[i].data, size));

		// One callback for each AIO buffer.
		assert(out_infos[i].aio_infos[0].callbacks == 1);
		assert(out_infos[i].aio_infos[1].callbacks == 1);
		assert(in_infos[i].aio_infos[0].callbacks == 1);
		assert(in_infos[i].aio_infos[1].callbacks == 1);

		// Correct size sent and received.
		assert(out_infos[i].aio_infos[0].size == size / 3);
		assert(out_infos[i].aio_infos[1].size == size - size / 3);
		assert(in_infos[i].aio_infos[0].size == size / 4);
		assert(in_infos[i].aio_infos[1].size == size - size / 4);

		// First batch of data should all be sent and received before the second batch
		for(size_t j = 0; j < nchannels; j++) {
			assert(timespec_lt(&out_infos[i].aio_infos[0].ts, &out_infos[j].aio_infos[1].ts));
			assert(timespec_lt(&in_infos[i].aio_infos[0].ts, &in_infos[j].aio_infos[1].ts));
		}

		// The non-AIO transfer should have completed before everything else
		assert(!timespec_lt(&out_infos[i].aio_infos[0].ts, &b_received_ts));
		assert(!timespec_lt(&in_infos[i].aio_infos[0].ts, &b_received_ts));
	}

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
}
