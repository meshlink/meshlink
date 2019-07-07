#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "../src/meshlink.h"
#include "utils.h"

static void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
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

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;

	return true;
}

static struct sync_flag poll_flag;
static size_t poll_len;

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	poll_len = len;
	set_sync_flag(&poll_flag, true);
}

static struct sync_flag receive_flag;
static size_t receive_len;

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)data;

	receive_len = len;
	set_sync_flag(&receive_flag, true);
}

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open three meshlink instances.

	meshlink_handle_t *mesh1 = meshlink_open("channels_failure_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	meshlink_handle_t *mesh2 = meshlink_open("channels_failure_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);

	assert(mesh1);
	assert(mesh2);

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, log_cb);
	meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, log_cb);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");
	meshlink_add_address(mesh2, "localhost");

	char *data1 = meshlink_export(mesh1);
	char *data2 = meshlink_export(mesh2);

	assert(data1);
	assert(data2);

	assert(meshlink_import(mesh1, data2));
	assert(meshlink_import(mesh2, data1));

	free(data1);
	free(data2);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh2, accept_cb);

	// Open a channel from foo to bar

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, receive_cb, NULL, 0);
	assert(channel);

	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb);

	// Start both instances

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	// Wait for the channel to be established

	assert(wait_sync_flag(&poll_flag, 10));
	assert(poll_len != 0);

	sleep(1);

	// Stop mesh2. We should get a notification that the channel has closed after a while.

	meshlink_stop(mesh2);

	assert(wait_sync_flag(&receive_flag, 70));
	assert(receive_len == 0);

	meshlink_channel_close(mesh1, channel);

	// Try setting up a new channel while bar is still down.

	poll_flag.flag = false;
	receive_flag.flag = false;

	channel = meshlink_channel_open(mesh1, bar, 7, NULL, NULL, 0);
	assert(channel);

	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb);

	assert(wait_sync_flag(&poll_flag, 70));
	assert(poll_len == 0);

	// Clean up.

	meshlink_close(mesh1);
	meshlink_close(mesh2);

	return 0;
}
