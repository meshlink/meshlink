#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <pthread.h>

#include "../src/meshlink.h"
#include "utils.h"

volatile bool b_responded = false;
volatile bool b_closed = false;
volatile bool a_nonzero_poll_cb = false;

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

void a_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		b_responded = true;
	} else if(len == 0) {
		b_closed = true;
	}
}

void b_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	// Send one message back, then close the channel.
	if(len) {
		meshlink_channel_send(mesh, channel, data, len);
	}

	meshlink_channel_close(mesh, channel);
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
	(void)port;

	meshlink_set_channel_accept_cb(mesh, NULL);
	meshlink_set_channel_receive_cb(mesh, channel, b_receive_cb);

	if(data) {
		b_receive_cb(mesh, channel, data, len);
	}

	return true;
}

void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	set_sync_flag(channel->priv, true);
}

void poll_cb2(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)mesh;
	(void)channel;

	if(len) {
		a_nonzero_poll_cb = true;
	}
}

int main() {
	meshlink_handle_t *a, *b;
	open_meshlink_pair(&a, &b, "channels-cornercases");
	//meshlink_set_log_cb(a, MESHLINK_DEBUG, log_cb);
	//meshlink_set_log_cb(b, MESHLINK_DEBUG, log_cb);

	// Set the callbacks.

	meshlink_set_channel_accept_cb(a, reject_cb);
	meshlink_set_channel_accept_cb(b, accept_cb);

	// Open a channel from a to b before starting the mesh.

	meshlink_node_t *nb = meshlink_get_node(a, "b");
	assert(nb);

	struct sync_flag channel_opened = {.flag = false};
	pthread_mutex_lock(&channel_opened.mutex);

	meshlink_channel_t *channel = meshlink_channel_open(a, nb, 7, a_receive_cb, NULL, 0);
	assert(channel);

	channel->priv = &channel_opened;
	meshlink_set_channel_poll_cb(a, channel, poll_cb);

	// Start MeshLink and wait for the channel to become connected.
	start_meshlink_pair(a, b);

	assert(wait_sync_flag(&channel_opened, 20));

	// Re-initialize everything
	close_meshlink_pair(a, b, "channels-cornercases");
	b_responded = false;
	b_closed = false;
	channel_opened.flag = false;
	open_meshlink_pair(&a, &b, "channels-cornercases");

	meshlink_set_channel_accept_cb(a, reject_cb);
	meshlink_set_channel_accept_cb(b, accept_cb);

	start_meshlink_pair(a, b);

	// Create a channel to b
	nb = meshlink_get_node(a, "b");
	assert(nb);

	channel = meshlink_channel_open(a, nb, 7, a_receive_cb, NULL, 0);
	assert(channel);
	channel->priv = &channel_opened;
	meshlink_set_channel_poll_cb(a, channel, poll_cb);

	assert(wait_sync_flag(&channel_opened, 20));

	assert(!b_responded);
	assert(!b_closed);

	// Send a message to b

	meshlink_channel_send(a, channel, "Hello", 5);

	sleep(1);

	assert(b_responded);
	assert(b_closed);

	// Try to create a second channel

	meshlink_channel_t *channel2 = meshlink_channel_open(a, nb, 7, a_receive_cb, NULL, 0);
	assert(channel2);
	meshlink_set_channel_poll_cb(a, channel2, poll_cb2);

	sleep(1);

	assert(!a_nonzero_poll_cb);

	return 0;
}
