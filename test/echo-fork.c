#define _GNU_SOURCE

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include "meshlink.h"
#include "utils.h"

/*
 * To run this test case, direct a large file to strd
 */

static struct sync_flag a_started;
static struct sync_flag a_stopped;
static struct sync_flag b_responded;

static void a_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)data;
	(void)len;

	// One way only.
}

static void b_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;

	if(!len) {
		set_sync_flag(&a_stopped, true);
		meshlink_channel_close(mesh, channel);
		return;
	}

	assert(write(1, data, len) == (ssize_t)len);
}

static bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)port;
	(void)data;
	(void)len;

	return false;
}

static bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	if(port != 7) {
		return false;
	}

	set_sync_flag(&a_started, true);

	meshlink_set_channel_receive_cb(mesh, channel, b_receive_cb);

	if(data) {
		b_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	set_sync_flag(&b_responded, true);
}

static int main1(void) {
	close(1);

	meshlink_handle_t *mesh = meshlink_open("echo-fork_conf.1", "a", "echo-fork", DEV_CLASS_BACKBONE);
	assert(mesh);

	meshlink_set_channel_accept_cb(mesh, reject_cb);

	assert(meshlink_start(mesh));

	// Open a channel.

	meshlink_node_t *b = meshlink_get_node(mesh, "b");
	assert(b);

	meshlink_channel_t *channel = meshlink_channel_open(mesh, b, 7, a_receive_cb, NULL, 0);
	assert(channel);

	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);

	// read and buffer stdin
	int BUF_SIZE = 1024 * 1024;
	char buffer[BUF_SIZE];

	assert(wait_sync_flag(&b_responded, 20));

	do {
		ssize_t len = read(0, buffer, BUF_SIZE);

		if(len <= 0) {
			break;
		}

		char *p = buffer;

		while(len > 0) {
			ssize_t sent = meshlink_channel_send(mesh, channel, p, len);

			if(sent < 0) {
				fprintf(stderr, "Sending message failed\n");
				return 1;
			}

			if(!sent) {
				usleep(100000);
			} else {
				len -= sent;
				p += sent;
			}
		}
	} while(true);

	meshlink_channel_close(mesh, channel);

	// Clean up.

	meshlink_close(mesh);

	return 0;
}


static int main2(void) {
#ifdef __linux__
	prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif

	close(0);

	// Start mesh and wait for incoming channels.

	meshlink_handle_t *mesh = meshlink_open("echo-fork_conf.2", "b", "echo-fork", DEV_CLASS_BACKBONE);
	assert(mesh);

	meshlink_set_channel_accept_cb(mesh, accept_cb);

	assert(meshlink_start(mesh));

	// Let it run until a disappears.

	assert(wait_sync_flag(&a_started, 20));
	assert(wait_sync_flag(&a_stopped, 1000000));

	// Clean up.

	meshlink_close(mesh);

	return 0;
}


int main(void) {
	init_sync_flag(&a_started);
	init_sync_flag(&a_stopped);
	init_sync_flag(&b_responded);

	meshlink_set_log_cb(NULL, MESHLINK_WARNING, log_cb);

	// Initialize and exchange configuration.

	meshlink_handle_t *mesh_a, *mesh_b;

	open_meshlink_pair(&mesh_a, &mesh_b, "echo-fork");
	close_meshlink_pair(mesh_a, mesh_b);

	if(!fork()) {
		return main2();
	}

	assert(main1() == 0);

	int wstatus = 0;
	assert(wait(&wstatus) != -1);
	assert(WIFEXITED(wstatus));
	assert(WEXITSTATUS(wstatus) == 0);
}
