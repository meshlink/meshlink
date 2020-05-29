#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

#include "meshlink.h"
#include "utils.h"

static size_t received;
static struct sync_flag recv_flag;
static struct sync_flag close_flag;
static struct sync_flag poll_flag;
static struct sync_flag aio_done_flag;

static void aio_fd_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, int fd, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)fd;
	(void)len;
	(void)priv;

	set_sync_flag(&aio_done_flag, true);
}

static void aio_fd_cb_ignore(meshlink_handle_t *mesh, meshlink_channel_t *channel, int fd, size_t len, void *priv) {
	(void)mesh;
	(void)channel;
	(void)fd;
	(void)len;
	(void)priv;
}

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)len;

	if(!data) {
		set_sync_flag(&close_flag, true);
		meshlink_channel_close(mesh, channel);
	}

	received += len;
	set_sync_flag(&recv_flag, true);
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)port;
	(void)data;
	(void)len;
	meshlink_set_channel_receive_cb(mesh, channel, receive_cb);
	return true;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	set_sync_flag(&poll_flag, true);
}

int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "channels_aio_fd");

	// Set the callbacks.

	meshlink_set_channel_accept_cb(mesh_b, accept_cb);

	// Start both instances

	start_meshlink_pair(mesh_a, mesh_b);

	// Open a channel from a to b.

	meshlink_node_t *b = meshlink_get_node(mesh_a, "b");
	assert(b);

	meshlink_channel_t *channel = meshlink_channel_open(mesh_a, b, 1, NULL, NULL, 0);
	assert(channel);

	// Wait for the channel to be fully established

	meshlink_set_channel_poll_cb(mesh_a, channel, poll_cb);
	assert(wait_sync_flag(&poll_flag, 10));

	// Create a UNIX stream socket

	int fds[2];
	assert(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
	assert(fcntl(fds[1], F_SETFL, O_NONBLOCK) == 0);

	// Enqueue 3 AIO buffers for the same fd

	assert(meshlink_channel_aio_fd_send(mesh_a, channel, fds[1], 200, aio_fd_cb, NULL));
	assert(meshlink_channel_aio_fd_send(mesh_a, channel, fds[1], 200, aio_fd_cb_ignore, NULL));
	assert(meshlink_channel_aio_fd_send(mesh_a, channel, fds[1], 200, aio_fd_cb, NULL));

	// Fill the first buffer with two packets

	char buf[65535] = "";

	sleep(1);
	assert(write(fds[0], buf, 100) == 100);
	assert(wait_sync_flag(&recv_flag, 2));
	assert(received == 100);

	sleep(1);
	assert(!check_sync_flag(&aio_done_flag));
	set_sync_flag(&recv_flag, false);
	assert(write(fds[0], buf, 100) == 100);
	assert(wait_sync_flag(&recv_flag, 2));
	assert(received == 200);

	assert(wait_sync_flag(&aio_done_flag, 1));
	set_sync_flag(&aio_done_flag, false);

	// Fill half of the second buffer

	set_sync_flag(&recv_flag, false);
	assert(write(fds[0], buf, 100) == 100);
	assert(wait_sync_flag(&recv_flag, 2));
	assert(received == 300);

	// Send one packet that spans two AIO buffers

	sleep(1);
	assert(!check_sync_flag(&aio_done_flag));
	assert(write(fds[0], buf, 300) == 300);
	assert(wait_sync_flag(&aio_done_flag, 10));

	// Close the channel and wait for the remaining data

	meshlink_channel_close(mesh_a, channel);
	assert(wait_sync_flag(&close_flag, 10));
	assert(received == 600);

	// Create a UDP channel

	channel = meshlink_channel_open_ex(mesh_a, b, 1, NULL, NULL, 0, MESHLINK_CHANNEL_UDP);
	assert(channel);

	// Wait for the channel to be fully established

	received = 0;
	set_sync_flag(&poll_flag, false);
	set_sync_flag(&recv_flag, false);
	set_sync_flag(&close_flag, false);
	meshlink_set_channel_poll_cb(mesh_a, channel, poll_cb);
	assert(wait_sync_flag(&poll_flag, 10));

	// Enqueue a huge AIO buffer

	set_sync_flag(&aio_done_flag, false);
	assert(meshlink_channel_aio_fd_send(mesh_a, channel, fds[1], -1, aio_fd_cb, NULL));

	// Send a small and a big packets

	assert(write(fds[0], buf, 100) == 100);
	assert(wait_sync_flag(&recv_flag, 2));
	assert(received == 100);

	sleep(1);
	assert(!check_sync_flag(&aio_done_flag));
	set_sync_flag(&recv_flag, false);
	assert(write(fds[0], buf, 65535) == 65535);
	assert(wait_sync_flag(&recv_flag, 2));
	assert(received == 65635);

	// Close the fds, this should terminate the AIO buffer

	sleep(1);
	assert(!check_sync_flag(&aio_done_flag));
	close(fds[0]);
	assert(wait_sync_flag(&aio_done_flag, 10));
	close(fds[1]);

	meshlink_channel_close(mesh_a, channel);
	assert(wait_sync_flag(&close_flag, 10));
	assert(received == 65635);

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
}
