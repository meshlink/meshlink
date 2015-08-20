#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "meshlink/meshlink.h"

/*
 * To run this test case, direct a large file to strd
 */

volatile bool bar_reachable = false;
volatile bool bar_responded = false;
volatile bool foo_closed = false;
int debug_level;

void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	if(mesh)
		fprintf(stderr, "(%s) ", mesh->name);
	fprintf(stderr, "[%d] %s\n", level, text);
}

void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	if(!strcmp(node->name, "bar"))
		bar_reachable = reachable;
	else if(!strcmp(node->name, "foo"))
		if(!reachable)
			foo_closed = true;
}

void foo_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	// One way only.
}

void bar_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	if(!len) {
		fprintf(stderr, "Connection closed by foo\n");
		foo_closed = true;
		return;
	}
	// Write data to stdout.
	write(1, data, len);
}

bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	return false;
}

bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	if(port != 7)
		return false;
	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);
	if(data)
		bar_receive_cb(mesh, channel, data, len);
	return true;
}

void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	bar_responded=true;
}

int main1(void) {
	close(1);

	meshlink_set_log_cb(NULL, debug_level, log_cb);

	meshlink_handle_t *mesh1 = meshlink_open("echo-fork_conf.1", "foo", "echo-fork", DEV_CLASS_BACKBONE);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	meshlink_set_log_cb(mesh1, debug_level, log_cb);
	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_node_status_cb(mesh1, status_cb);

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return 1;
	}

	for(int i = 0; i < 20; i++) {
		sleep(1);
		if(bar_reachable)
			break;
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

	// read and buffer stdin
	int BUF_SIZE = 1024*1024;
	char buffer[BUF_SIZE];

	for(int i = 0; i < 5; i++) {
		sleep(1);
		if(bar_responded)
			break;
	}

	if(!bar_responded) {
		fprintf(stderr, "Bar did not respond to foo's channel message\n");
		return 1;
	}

	do {
		//fprintf(stderr, ":");
		ssize_t len = read(0, buffer, BUF_SIZE);
		if(len <= 0)
			break;
		char *p = buffer;
		while(len > 0) {
			ssize_t sent = meshlink_channel_send(mesh1, channel, p, len);
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

	fprintf(stderr, "Foo finished sending\n");

	meshlink_channel_close(mesh1, channel);
	sleep(1);

	// Clean up.

	meshlink_close(mesh1);

	return 0;
}


int main2(void) {
	close(0);

	sleep(1);

	meshlink_set_log_cb(NULL, debug_level, log_cb);

	meshlink_handle_t *mesh2 = meshlink_open("echo-fork_conf.2", "bar", "echo-fork", DEV_CLASS_BACKBONE);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return 1;
	}

	meshlink_set_log_cb(mesh2, debug_level, log_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb);
	meshlink_set_node_status_cb(mesh2, status_cb);

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return 1;
	}

	while(!foo_closed)
		sleep(1);

	// Clean up.

	meshlink_close(mesh2);

	return 0;
}


int main(int argc, char *argv[]) {
	debug_level = getenv("DEBUG") ? MESHLINK_DEBUG : MESHLINK_ERROR;

	// Initialize and exchange configuration.

	meshlink_handle_t *foo = meshlink_open("echo-fork_conf.1", "foo", "echo-fork", DEV_CLASS_BACKBONE);
	meshlink_handle_t *bar = meshlink_open("echo-fork_conf.2", "bar", "echo-fork", DEV_CLASS_BACKBONE);
	meshlink_add_address(foo, "localhost");
	meshlink_import(bar, meshlink_export(foo));
	meshlink_import(foo, meshlink_export(bar));
	meshlink_close(foo);
	meshlink_close(bar);

	if(fork())
		return main1();
	else
		return main2();
}
