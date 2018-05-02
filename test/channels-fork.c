#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "../src/meshlink.h"

volatile bool bar_reachable = false;
volatile bool bar_responded = false;

void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
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

void foo_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
	(void)len;

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		bar_responded = true;
	}
}

void bar_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	// Echo the data back.
	meshlink_channel_send(mesh, channel, data, len);
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
	if(port != 7) {
		return false;
	}

	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);

	if(data) {
		bar_receive_cb(mesh, channel, data, len);
	}

	return true;
}

void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	(void)len;

	meshlink_set_channel_poll_cb(mesh, channel, NULL);

	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5) {
		fprintf(stderr, "Could not send whole message\n");
	}
}

int main1(int rfd, int wfd) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	meshlink_handle_t *mesh1 = meshlink_open("channels_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);

	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	meshlink_enable_discovery(mesh1, false);

	meshlink_add_address(mesh1, "localhost");

	char *data = meshlink_export(mesh1);

	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return 1;
	}

	size_t len = strlen(data);
	write(wfd, &len, sizeof(len));
	write(wfd, data, len);
	free(data);

	read(rfd, &len, sizeof(len));
	char indata[len + 1];
	read(rfd, indata, len);
	indata[len] = 0;

	fprintf(stderr, "Foo exchanged data\n");

	meshlink_import(mesh1, indata);

	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_node_status_cb(mesh1, status_cb);

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return 1;
	}

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(bar_reachable) {
			break;
		}
	}

	if(!bar_reachable) {
		fprintf(stderr, "Bar not reachable for foo after 20 seconds\n");
		return 1;
	}

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");

	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return 1;
	}

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb);

	for(int i = 0; i < 5; i++) {
		sleep(1);

		if(bar_responded) {
			break;
		}
	}

	if(!bar_responded) {
		fprintf(stderr, "Bar did not respond to foo's channel message\n");
		return 1;
	}

	meshlink_channel_close(mesh1, channel);

	// Clean up.

	meshlink_close(mesh1);

	return 0;
}


int main2(int rfd, int wfd) {
	sleep(1);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	meshlink_handle_t *mesh2 = meshlink_open("channels_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);

	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return 1;
	}

	meshlink_enable_discovery(mesh2, false);

	char *data = meshlink_export(mesh2);

	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return 1;
	}

	size_t len = strlen(data);

	if(write(wfd, &len, sizeof(len)) <= 0) {
		abort();
	}

	if(write(wfd, data, len) <= 0) {
		abort();
	}

	free(data);

	read(rfd, &len, sizeof(len));
	char indata[len + 1];
	read(rfd, indata, len);
	indata[len] = 0;

	fprintf(stderr, "Bar exchanged data\n");

	meshlink_import(mesh2, indata);

	meshlink_set_channel_accept_cb(mesh2, accept_cb);

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return 1;
	}

	sleep(20);

	// Clean up.

	meshlink_close(mesh2);

	return 0;
}


int main() {
	int fda[2], fdb[2];

	pipe2(fda, 0);
	pipe2(fdb, 0);

	if(fork()) {
		return main1(fda[0], fdb[1]);
	} else {
		return main2(fdb[0], fda[1]);
	}
}
