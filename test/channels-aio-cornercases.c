#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "meshlink.h"
#include "utils.h"

static const size_t size = 10000000; // size of data to transfer

struct aio_info {
	int port;
	int callbacks;
	size_t size;
	struct timeval tv;
	struct sync_flag flag;
};

struct channel_info {
	char *data;
	struct aio_info aio_infos[2];
};

static size_t b_received_len;
static struct timeval b_received_tv;
static struct sync_flag b_received_flag;

static void aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;

	struct aio_info *info = priv;

	fprintf(stderr, "%d:%s aio_cb %s %p %zu\n", info->port, mesh->name, channel->node->name, data, len);

	gettimeofday(&info->tv, NULL);
	info->callbacks++;
	info->size += len;
	set_sync_flag(&info->flag, true);
}

static void aio_cb_close(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	aio_cb(mesh, channel, data, len, priv);
	struct aio_info *info = priv;
	fprintf(stderr, "%d:%s aio_cb %s closing\n", info->port, mesh->name, channel->node->name);
	meshlink_channel_close(mesh, channel);
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	assert(!data);
	assert(!len);

	fprintf(stderr, "%d:%s accept_cb %s\n", port, mesh->name, channel->node->name);

	struct channel_info *infos = mesh->priv;
	struct channel_info *info = &infos[port - 1];

	switch(port) {
	case 1:
	case 3:
		meshlink_channel_aio_receive(mesh, channel, info->data, size / 4, aio_cb, &info->aio_infos[0]);
		meshlink_channel_aio_receive(mesh, channel, info->data + size / 4, size - size / 4, aio_cb_close, &info->aio_infos[1]);
		break;

	case 2:
	case 4:
		meshlink_channel_aio_receive(mesh, channel, info->data, size / 4, aio_cb_close, &info->aio_infos[0]);
		meshlink_channel_aio_receive(mesh, channel, info->data + size / 4, size - size / 4, aio_cb, &info->aio_infos[1]);
		set_sync_flag(&info->aio_infos[1].flag, true);
		break;

	default:
		return false;
	}

	return true;
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	// Prepare data buffers

	char *outdata = malloc(size);
	assert(outdata);

	for(size_t i = 0; i < size; i++) {
		outdata[i] = i;
	}

	static const size_t nchannels = 4;
	struct channel_info in_infos[nchannels];
	struct channel_info out_infos[nchannels];

	memset(in_infos, 0, sizeof(in_infos));
	memset(out_infos, 0, sizeof(out_infos));

	for(size_t i = 0; i < nchannels; i++) {
		in_infos[i].data = malloc(size);
		assert(in_infos[i].data);
		out_infos[i].data = outdata;

		out_infos[i].aio_infos[0].port = i + 1;
		out_infos[i].aio_infos[1].port = i + 1;
		in_infos[i].aio_infos[0].port = i + 1;
		in_infos[i].aio_infos[1].port = i + 1;
	}

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels_aio_cornercases");

	// Set the callbacks.

	mesh_b->priv = in_infos;

	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	// Start both instances

	start_meshlink_pair(mesh_a, mesh_b);
	sleep(1);

	// Open channels from a to b.

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channels[nchannels + 1];

	// Send a large buffer of data on each channel.

	for(size_t i = 0; i < nchannels; i++) {
		channels[i] = meshlink_channel_open(mesh_a, b, i + 1, NULL, NULL, 0);
		assert(channels[i]);

		if(i < 2) {
			assert(meshlink_channel_aio_send(mesh_a, channels[i], outdata, size / 3, aio_cb, &out_infos[i].aio_infos[0]));
			assert(meshlink_channel_aio_send(mesh_a, channels[i], outdata + size / 3, size - size / 3, aio_cb_close, &out_infos[i].aio_infos[1]));
			assert(wait_sync_flag(&out_infos[i].aio_infos[0].flag, 10));
			assert(wait_sync_flag(&out_infos[i].aio_infos[1].flag, 10));
		} else {
			assert(meshlink_channel_aio_send(mesh_a, channels[i], outdata, size / 3, aio_cb_close, &out_infos[i].aio_infos[0]));
			assert(meshlink_channel_aio_send(mesh_a, channels[i], outdata + size / 3, size - size / 3, aio_cb, &out_infos[i].aio_infos[1]));
			assert(wait_sync_flag(&out_infos[i].aio_infos[0].flag, 10));
			set_sync_flag(&out_infos[i].aio_infos[1].flag, true);
		}
	}

	// Wait for all AIO buffers to finish.

	for(size_t i = 0; i < nchannels; i++) {
		assert(wait_sync_flag(&in_infos[i].aio_infos[0].flag, 10));
		assert(wait_sync_flag(&in_infos[i].aio_infos[1].flag, 10));
		assert(wait_sync_flag(&out_infos[i].aio_infos[0].flag, 10));
		assert(wait_sync_flag(&out_infos[i].aio_infos[1].flag, 10));
	}

	// Check that everything is correct.

	assert(!memcmp(in_infos[0].data, out_infos[0].data, size));
	assert(!memcmp(in_infos[1].data, out_infos[1].data, size / 4));
	assert(memcmp(in_infos[1].data, out_infos[1].data + size / 4, size - size / 4));
	assert(!memcmp(in_infos[2].data, out_infos[2].data, size / 3));
	assert(memcmp(in_infos[2].data, out_infos[2].data + size / 3, size - size / 3));
	assert(!memcmp(in_infos[3].data, out_infos[3].data, size / 4));
	assert(memcmp(in_infos[3].data, out_infos[3].data + size / 4, size / 3 - size / 4));
	assert(memcmp(in_infos[3].data, out_infos[3].data + size / 3, size - size / 3));

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);

	free(outdata);

	for(size_t i = 0; i < nchannels; i++) {
		free(in_infos[i].data);
	}
}
