#ifdef NDEBUG
#undef NDEBUG
#endif

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "meshlink.h"
#include "utils.h"

static const size_t size = 1024 * 1024; // size of data to transfer
static const size_t nchannels = 4; // number of simultaneous channels

struct aio_info {
	int callbacks;
	size_t size;
	struct timespec ts;
	struct sync_flag flag;
};

struct channel_info {
	FILE *file;
	struct aio_info aio_infos[2];
};

static void aio_fd_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, int fd, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)fd;
	(void)len;

	struct aio_info *info = priv;
	clock_gettime(CLOCK_MONOTONIC, &info->ts);
	info->callbacks++;
	info->size += len;
	set_sync_flag(&info->flag, true);
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	assert(port && port <= nchannels);
	assert(!data);
	assert(!len);

	struct channel_info *infos = mesh->priv;
	struct channel_info *info = &infos[port - 1];

	assert(meshlink_channel_aio_fd_receive(mesh, channel, fileno(info->file), size / 4, aio_fd_cb, &info->aio_infos[0]));
	assert(meshlink_channel_aio_fd_receive(mesh, channel, fileno(info->file), size - size / 4, aio_fd_cb, &info->aio_infos[1]));

	return true;
}

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	// Prepare file

	char *outdata = malloc(size);
	assert(outdata);

	for(size_t i = 0; i < size; i++) {
		// Human readable output
		outdata[i] = i % 96 ? i % 96 + 32 : '\n';
	}

	FILE *file = fopen("channels_aio_fd.in", "w");
	assert(file);
	assert(fwrite(outdata, size, 1, file) == 1);
	assert(fclose(file) == 0);

	struct channel_info in_infos[nchannels];
	struct channel_info out_infos[nchannels];

	memset(in_infos, 0, sizeof(in_infos));
	memset(out_infos, 0, sizeof(out_infos));

	for(size_t i = 0; i < nchannels; i++) {
		init_sync_flag(&in_infos[i].aio_infos[0].flag);
		init_sync_flag(&in_infos[i].aio_infos[1].flag);
		init_sync_flag(&out_infos[i].aio_infos[0].flag);
		init_sync_flag(&out_infos[i].aio_infos[1].flag);

		char filename[PATH_MAX];
		snprintf(filename, sizeof(filename), "channels_aio_fd.out%d", (int)i);
		in_infos[i].file = fopen(filename, "w");
		assert(in_infos[i].file);
		out_infos[i].file = fopen("channels_aio_fd.in", "r");
		assert(out_infos[i].file);
	}

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels_aio_fd");

	mesh_b->priv = in_infos;

	meshlink_enable_discovery(mesh_a, false);
	meshlink_enable_discovery(mesh_b, false);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	// Start both instances

	start_meshlink_pair(mesh_a, mesh_b);

	// Open channels from a to b.

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channels[nchannels];

	for(size_t i = 0; i < nchannels; i++) {
		channels[i] = meshlink_channel_open(mesh_a, b, i + 1, NULL, NULL, 0);
		assert(channels[i]);
	}

	// Send a large buffer of data on each channel.

	for(size_t i = 0; i < nchannels; i++) {
		assert(meshlink_channel_aio_fd_send(mesh_a, channels[i], fileno(out_infos[i].file), size / 3, aio_fd_cb, &out_infos[i].aio_infos[0]));
		assert(meshlink_channel_aio_fd_send(mesh_a, channels[i], fileno(out_infos[i].file), size - size / 3, aio_fd_cb, &out_infos[i].aio_infos[1]));
	}

	// Wait for everyone to finish.

	for(size_t i = 0; i < nchannels; i++) {
		assert(wait_sync_flag(&out_infos[i].aio_infos[0].flag, 10));
		assert(wait_sync_flag(&out_infos[i].aio_infos[1].flag, 10));
		assert(wait_sync_flag(&in_infos[i].aio_infos[0].flag, 10));
		assert(wait_sync_flag(&in_infos[i].aio_infos[1].flag, 10));
	}

	// Check that everything is correct.

	for(size_t i = 0; i < nchannels; i++) {
		assert(fclose(in_infos[i].file) == 0);
		assert(fclose(out_infos[i].file) == 0);

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

		// Files should be identical
		char command[PATH_MAX];
		snprintf(command, sizeof(command), "cmp channels_aio_fd.in channels_aio_fd.out%d", (int)i);
		assert(system(command) == 0);

	}

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
	free(outdata);
}
