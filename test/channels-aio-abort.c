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

#include "meshlink.h"
#include "utils.h"

static const size_t size = 2000000; // size of data to transfer

struct aio_info {
	char *data;
	int callbacks;
	size_t size;
	struct timespec ts;
	struct sync_flag flag;
};

static void aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	fprintf(stderr, "%s aio_cb %s %p %zu\n", mesh->name, channel->node->name, data, len);
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;

	struct aio_info *info = priv;
	clock_gettime(CLOCK_MONOTONIC, &info->ts);
	info->callbacks++;
	info->size += len;
	set_sync_flag(&info->flag, true);
	meshlink_channel_abort(mesh, channel);
	free(info->data);
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	fprintf(stderr, "%s accept %s\n", mesh->name, channel->node->name);
	assert(port == 1);
	assert(!data);
	assert(!len);

	struct aio_info *info = mesh->priv;

	assert(meshlink_channel_aio_receive(mesh, channel, info->data, size / 2, aio_cb, info));

	return true;
}

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	struct aio_info in_info;
	struct aio_info out_info;

	memset(&in_info, 0, sizeof(in_info));
	memset(&out_info, 0, sizeof(out_info));

	init_sync_flag(&in_info.flag);
	init_sync_flag(&out_info.flag);

	in_info.data = calloc(1, size / 2);
	assert(in_info.data);
	out_info.data = calloc(1, size);
	assert(out_info.data);

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels_aio_abort");

	// Set the callbacks.

	mesh_b->priv = &in_info;
	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	// Start both instances

	start_meshlink_pair(mesh_a, mesh_b);

	// Open channel from a to b.

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);
	meshlink_channel_t *channel = meshlink_channel_open(mesh_a, b, 1, NULL, NULL, 0);
	assert(channel);

	// Send data, receiver aborts halfway

	assert(meshlink_channel_aio_send(mesh_a, channel, out_info.data, size, aio_cb, &out_info));

	// Wait for everyone to finish.

	assert(wait_sync_flag(&out_info.flag, 10));
	assert(wait_sync_flag(&in_info.flag, 10));

	// Open a new data, now sender aborts halfway

	init_sync_flag(&in_info.flag);
	init_sync_flag(&out_info.flag);

	in_info.data = calloc(1, size / 2);
	assert(in_info.data);
	out_info.data = calloc(1, size / 4);
	assert(out_info.data);

	channel = meshlink_channel_open(mesh_a, b, 1, NULL, NULL, 0);
	assert(channel);
	assert(meshlink_channel_aio_send(mesh_a, channel, out_info.data, size / 4, aio_cb, &out_info));

	// Wait for everyone to finish.

	assert(wait_sync_flag(&out_info.flag, 10));
	assert(wait_sync_flag(&in_info.flag, 10));

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
}
