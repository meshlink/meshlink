#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "meshlink.h"
#include "utils.h"

static const size_t size = 25000000; // size of data to transfer
static const size_t smallsize = 100000; // size of the data to transfer without AIO
static const size_t nchannels = 4; // number of simultaneous channels

struct aio_info {
	int callbacks;
	size_t size;
	struct timeval tv;
	struct sync_flag flag;
};

struct channel_info {
	char *data;
	struct aio_info aio_infos[2];
};

static size_t bar_received_len;
static struct timeval bar_received_tv;
static struct sync_flag bar_received_flag;

static void aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;

	struct aio_info *info = priv;
	gettimeofday(&info->tv, NULL);
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

	bar_received_len += len;

	if(bar_received_len >= smallsize) {
		gettimeofday(&bar_received_tv, NULL);
		set_sync_flag(&bar_received_flag, true);
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

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

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
		in_infos[i].data = malloc(size);
		assert(in_infos[i].data);
		out_infos[i].data = outdata;
	}

	// Open two new meshlink instance.

	meshlink_destroy("channels_aio_conf.1");
	meshlink_destroy("channels_aio_conf.2");

	meshlink_handle_t *mesh1 = meshlink_open("channels_aio_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1);

	meshlink_handle_t *mesh2 = meshlink_open("channels_aio_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2);

	mesh2->priv = in_infos;

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *data = meshlink_export(mesh1);
	assert(data);
	assert(meshlink_import(mesh2, data));
	free(data);

	data = meshlink_export(mesh2);
	assert(data);
	assert(meshlink_import(mesh1, data));
	free(data);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb);

	// Start both instances

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	// Open channels from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);

	meshlink_channel_t *channels[nchannels + 1];

	for(size_t i = 0; i < nchannels + 1; i++) {
		channels[i] = meshlink_channel_open(mesh1, bar, i + 1, NULL, NULL, 0);
		assert(channels[i]);
	}

	// Send a large buffer of data on each channel.

	for(size_t i = 0; i < nchannels; i++) {
		assert(meshlink_channel_aio_send(mesh1, channels[i], outdata, size / 3, aio_cb, &out_infos[i].aio_infos[0]));
		assert(meshlink_channel_aio_send(mesh1, channels[i], outdata + size / 3, size - size / 3, aio_cb, &out_infos[i].aio_infos[1]));
	}

	// Send a little bit on the last channel using a regular send

	assert(meshlink_channel_send(mesh1, channels[nchannels], outdata, smallsize) == smallsize);

	// Wait for everyone to finish.

	assert(wait_sync_flag(&bar_received_flag, 10));

	for(size_t i = 0; i < nchannels; i++) {
		assert(wait_sync_flag(&out_infos[i].aio_infos[0].flag, 10));
		assert(wait_sync_flag(&out_infos[i].aio_infos[1].flag, 10));
		assert(wait_sync_flag(&in_infos[i].aio_infos[0].flag, 10));
		assert(wait_sync_flag(&in_infos[i].aio_infos[1].flag, 10));
	}

	// Check that everything is correct.

	assert(bar_received_len == smallsize);

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
			assert(timercmp(&out_infos[i].aio_infos[0].tv, &out_infos[j].aio_infos[1].tv, <=));
			assert(timercmp(&in_infos[i].aio_infos[0].tv, &in_infos[j].aio_infos[1].tv, <=));
		}

		// The non-AIO transfer should have completed before everything else
		assert(timercmp(&out_infos[i].aio_infos[0].tv, &bar_received_tv, >=));
		assert(timercmp(&in_infos[i].aio_infos[0].tv, &bar_received_tv, >=));
	}

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);

	return 0;
}
