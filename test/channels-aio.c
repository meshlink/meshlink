#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "meshlink.h"
#include "utils.h"

static const size_t size = 10000000; // size of data to transfer
static bool bar_reachable = false;
static int foo_callbacks = 0;
static size_t bar_received = 0;
static struct sync_flag bar_finished_flag;

void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	static struct timeval tv0;
	struct timeval tv;

	if(tv0.tv_sec == 0) {
		gettimeofday(&tv0, NULL);
	}

	gettimeofday(&tv, NULL);
	fprintf(stderr, "%u.%.03u ", (unsigned int)(tv.tv_sec - tv0.tv_sec), (unsigned int)tv.tv_usec / 1000);

	if(mesh) {
		fprintf(stderr, "(%s) ", mesh->name);
	}

	fprintf(stderr, "[%d] %s\n", level, text);
}

void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!strcmp(node->name, "bar")) {
		bar_reachable = reachable;
	}
}

void foo_aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;
	(void)priv;

	foo_callbacks++;
}

void bar_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)channel;

	char *indata = mesh->priv;
	memcpy(indata, data, len);
	mesh->priv = indata + len;
	bar_received += len;

	if(bar_received >= size) {
		set_sync_flag(&bar_finished_flag, true);
	}
}

bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;

	return false;
}

bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	assert(port == 7);

	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);

	if(data) {
		bar_receive_cb(mesh, channel, data, len);
	}

	return true;
}

void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	assert(meshlink_channel_send(mesh, channel, "Hello", 5) == 5);
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	// Prepare data buffers

	char *outdata = malloc(size);
	char *indata = malloc(size);

	assert(outdata);
	assert(indata);

	for(size_t i = 0; i < size; i++) {
		outdata[i] = i;
	}

	memset(indata, 0, size);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two new meshlink instance.

	meshlink_destroy("channels_aio_conf.1");
	meshlink_destroy("channels_aio_conf.2");

	meshlink_handle_t *mesh1 = meshlink_open("channels_aio_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1);

	meshlink_handle_t *mesh2 = meshlink_open("channels_aio_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2);

	mesh2->priv = indata;

	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, log_cb);
	meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, log_cb);

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

	meshlink_set_node_status_cb(mesh1, status_cb);

	// Start both instances

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, NULL, NULL, 0);
	assert(channel);

	// Send a large buffer of data.

	assert(meshlink_channel_aio_send(mesh1, channel, outdata, size / 2, foo_aio_cb, NULL));
	assert(meshlink_channel_aio_send(mesh1, channel, outdata + size / 2, size - size / 2, foo_aio_cb, NULL));

	assert(wait_sync_flag(&bar_finished_flag, 10));

	assert(foo_callbacks == 2);
	assert(bar_received == size);
	assert(!memcmp(indata, outdata, size));

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);

	return 0;
}
