#define _GNU_SOURCE 1

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#ifdef __linux__
#include <sys/prctl.h>
#endif

#include "utils.h"
#include "../src/meshlink.h"

static struct sync_flag bar_responded;
static struct sync_flag foo_connected;
static struct sync_flag foo_gone;

static void foo_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)len;

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		set_sync_flag(&bar_responded, true);
	}
}

static void bar_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!strcmp(node->name, "foo") && !reachable) {
		set_sync_flag(&foo_gone, true);
	}
}

static void bar_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	// Echo the data back.
	if(len) {
		assert(meshlink_channel_send(mesh, channel, data, len) == (ssize_t)len);
	} else {
		meshlink_channel_close(mesh, channel);
	}
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

	meshlink_set_node_status_cb(mesh, bar_status_cb);
	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);
	set_sync_flag(&foo_connected, true);

	if(data) {
		bar_receive_cb(mesh, channel, data, len);
	}

	return true;
}

static void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5) {
		fprintf(stderr, "Could not send whole message\n");
	}
}

static int main1(int rfd, int wfd) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	assert(meshlink_destroy("channels_fork_conf.1"));
	meshlink_handle_t *mesh = meshlink_open("channels_fork_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh);

	meshlink_enable_discovery(mesh, false);

	assert(meshlink_set_canonical_address(mesh, meshlink_get_self(mesh), "localhost", NULL));

	char *data = meshlink_export(mesh);
	assert(data);

	ssize_t len = strlen(data);
	assert(write(wfd, &len, sizeof(len)) == sizeof(len));
	assert(write(wfd, data, len) == len);
	free(data);

	assert(read(rfd, &len, sizeof(len)) == sizeof(len));
	char indata[len + 1];
	assert(read(rfd, indata, len) == len);
	indata[len] = 0;

	assert(meshlink_import(mesh, indata));

	meshlink_set_channel_accept_cb(mesh, reject_cb);

	assert(meshlink_start(mesh));

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh, "bar");
	assert(bar);

	meshlink_channel_t *channel = meshlink_channel_open(mesh, bar, 7, foo_receive_cb, NULL, 0);
	assert(channel);

	meshlink_set_channel_poll_cb(mesh, channel, poll_cb);

	assert(wait_sync_flag(&bar_responded, 20));

	meshlink_channel_close(mesh, channel);

	// Clean up.

	meshlink_close(mesh);

	return 0;
}


static int main2(int rfd, int wfd) {
#ifdef __linux__
	prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	assert(meshlink_destroy("channels_fork_conf.2"));
	meshlink_handle_t *mesh = meshlink_open("channels_fork_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh);

	meshlink_enable_discovery(mesh, false);

	assert(meshlink_set_canonical_address(mesh, meshlink_get_self(mesh), "localhost", NULL));

	char *data = meshlink_export(mesh);
	assert(data);

	ssize_t len = strlen(data);
	assert(write(wfd, &len, sizeof(len)) == sizeof(len));
	assert(write(wfd, data, len) == len);
	free(data);

	assert(read(rfd, &len, sizeof(len)) == sizeof(len));
	char indata[len + 1];
	assert(read(rfd, indata, len) == len);
	indata[len] = 0;

	assert(meshlink_import(mesh, indata));

	meshlink_set_channel_accept_cb(mesh, accept_cb);

	assert(meshlink_start(mesh));

	assert(wait_sync_flag(&foo_connected, 20));
	assert(wait_sync_flag(&foo_gone, 20));

	meshlink_close(mesh);

	return 0;
}

static void alarm_handler(int sig) {
	(void)sig;
	assert(0);
}

int main() {
	int fda[2], fdb[2];

	assert(pipe2(fda, 0) != -1);
	assert(pipe2(fdb, 0) != -1);

	if(!fork()) {
		return main2(fdb[0], fda[1]);
	}

	signal(SIGALRM, alarm_handler);
	alarm(30);
	assert(main1(fda[0], fdb[1]) == 0);

	int wstatus;
	assert(wait(&wstatus) != -1 || errno == ECHILD);
	assert(WIFEXITED(wstatus));
	assert(WEXITSTATUS(wstatus) == 0);
}
