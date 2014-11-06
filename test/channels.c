#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "../src/meshlink.h"
#include "../src/node.h"

volatile bool bar_reachable = false;
volatile bool bar_responded = false;

void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	static struct timeval tv0;
	struct timeval tv;

	if(tv0.tv_sec == 0)
		gettimeofday(&tv0, NULL);
	gettimeofday(&tv, NULL);
	fprintf(stderr, "%u.%.03u ", (unsigned int)(tv.tv_sec-tv0.tv_sec), (unsigned int)tv.tv_usec/1000);

	if(mesh)
		fprintf(stderr, "(%s) ", mesh->name);
	fprintf(stderr, "[%d] %s\n", level, text);
}

void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	printf("status_cb: %s %sreachable\n", node->name, reachable?"":"un");
	if(!strcmp(node->name, "bar"))
		bar_reachable = reachable;
}

void foo_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	printf("foo_receive_cb %zu: ", len); fwrite(data, 1, len, stdout); printf("\n");
	if(len == 5 && !memcmp(data, "Hello", 5))
		bar_responded = true;
}

void bar_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	printf("bar_receive_cb %zu: ", len); fwrite(data, 1, len, stdout);
	// Echo the data back.
	meshlink_channel_send(mesh, channel, data, len);
}

bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	return false;
}

bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	printf("accept_cb: (from %s on port %u) ", channel->node->name, (unsigned int)port);
	if(data) { fwrite(data, 1, len, stdout); printf("\n"); }

	if(port != 7)
		return false;
	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);
	if(data)
		bar_receive_cb(mesh, channel, data, len);
	return true;
}

void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5)
		fprintf(stderr, "Could not send whole message\n");
}

int main(int argc, char *argv[]) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("channels_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	meshlink_handle_t *mesh2 = meshlink_open("channels_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return 1;
	}

	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, log_cb);
	meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, log_cb);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

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

	// Set the callbacks.
	
	meshlink_set_channel_accept_cb(mesh1, reject_cb);
	meshlink_set_channel_accept_cb(mesh2, accept_cb);

	meshlink_set_node_status_cb(mesh1, status_cb);
	
	// Start both instances

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

	// XXX not enough to wait for reachable, must wait for SPTPS to complete
	for(int i=0; i < 20; i++) {
		sleep(1);
		if(((node_t *)bar)->status.validkey)
			break;
	}
	if(!((node_t *)bar)->status.validkey) {
		fprintf(stderr, "No key exchange after 20 seconds\n");
		return 1;
	}

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	meshlink_set_channel_poll_cb(mesh1, channel, poll_cb);

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
