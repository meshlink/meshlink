#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "meshlink.h"

volatile bool bar_reachable = false;
volatile bool bar_responded = false;

void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	if(!strcmp(node->name, "bar"))
		bar_reachable = reachable;
}

void foo_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	if(len == 5 && !memcmp(data, "Hello", 5))
		bar_responded = true;
}

void bar_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	// Echo the data back.
	meshlink_channel_send(mesh, channel, data, len);
}

bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, meshlink_node_t *node, uint16_t port, const void *data, size_t len) {
	if(port != 7)
		return false;
	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);
	if(data)
		bar_receive_cb(mesh, channel, data, len);
	return true;
}

int main(int argc, char *argv[]) {
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("channels_conf.1", "foo", "channels");
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	meshlink_handle_t *mesh2 = meshlink_open("channels_conf.2", "bar", "channels");
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return 1;
	}

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");
	meshlink_add_address(mesh2, "localhost");

	char *data = meshlink_export(mesh1);
	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return 1;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return 1;
	}

	free(data);

	data = meshlink_export(mesh2);
	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return 1;
	}


	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return 1;
	}

	free(data);

	// Set the channel accept callback on bar.
	
	meshlink_set_channel_accept_cb(mesh2, accept_cb);

	// Start both instances

	meshlink_set_node_status_cb(mesh1, status_cb);
	
	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return 1;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return 1;
	}

	// Wait for the two to connect.

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

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, "Hello", 5);

	for(int i = 0; i < 5; i++) {
		sleep(1);
		if(bar_responded)
			break;
	}

	if(!bar_responded) {
		fprintf(stderr, "Bar did not respond to foo's channel message\n");
		return 1;
	}

	meshlink_channel_close(mesh1, channel);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);

	return 0;
}
