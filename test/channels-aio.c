#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "meshlink/meshlink.h"
#include "../src/node.h"

static const size_t size = 10000000; // size of data to transfer
volatile bool bar_reachable = false;
volatile int foo_callbacks = 0;
volatile int bar_callbacks = 0;

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
	fprintf(stderr, "status_cb: %s %sreachable\n", node->name, reachable?"":"un");
	if(!strcmp(node->name, "bar"))
		bar_reachable = reachable;
}

void foo_aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, void *data, size_t len, void *priv) {
	fprintf(stderr, "foo_aio_cb %p %lu %p\n", data, (unsigned long)len, priv);
	foo_callbacks++;
}

void bar_aio_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, void *data, size_t len, void *priv) {
	fprintf(stderr, "bar_aio_cb %p %lu %p\n", data, (unsigned long)len, priv);
	bar_callbacks++;
	if(!priv) { // Expecting a size_t equal to size
		if(len != sizeof(size_t)) {
			fprintf(stderr, "Unexpected data length\n");
			exit(1);
		}
		if(*(size_t *)data != size) {
			fprintf(stderr, "Unexpected data: %lu\n", (unsigned long)(*(size_t *)data));
			exit(1);
		}

		// We can immediately enqueue another AIO buffer in the callback.
		if(!meshlink_channel_aio_receive(mesh, channel, mesh->priv, size, bar_aio_cb, mesh->priv)) {
			fprintf(stderr, "meshlink_channel_aio_receive(): %s\n", meshlink_strerror(errno));
			exit(1);
		}
	} else if(priv == mesh->priv) { // The actual data.
		if(len != size) {
			fprintf(stderr, "Unexpected data length\n");
			exit(1);
		}
	} else {
		fprintf(stderr, "Unexpected value for priv\n");
		exit(1);
	}
}

void bar_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	if(!len) {
		meshlink_channel_close(mesh, channel);
		return ;
	}
	fprintf(stderr, "bar got %lu unexpected bytes via receive_cb\n", (unsigned long)len);
	exit(1);
}

bool reject_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	return false;
}

bool accept_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	fprintf(stderr, "accept_cb: (from %s on port %u)\n", channel->node->name, (unsigned int)port);

	if(port != 7)
		return false;

	if(data || len) {
		fprintf(stderr, "Cannot handle data in accept_cb()\n");
		exit(1);
	}

	meshlink_set_channel_receive_cb(mesh, channel, bar_receive_cb);

	// Enqueue an AIO buffer for receiving the amount of data to be received.
	char *indata = malloc(sizeof(size_t));
	meshlink_channel_aio_receive(mesh, channel, indata, sizeof(size_t), bar_aio_cb, NULL);

	return true;
}

void poll_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	if(meshlink_channel_send(mesh, channel, "Hello", 5) != 5)
		fprintf(stderr, "Could not send whole message\n");
}

int main(int argc, char *argv[]) {
	// Prepare data buffers

	char *outdata = malloc(size);
	char *indata = malloc(size);

	if(!indata || !outdata) {
		fprintf(stderr, "Could not allocate memory\n");
		return 1;
	}

	for(size_t i = 0; i < size; i++)
		outdata[i] = i;

	memset(indata, 0, size);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("channels_aio_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	meshlink_handle_t *mesh2 = meshlink_open("channels_aio_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return 1;
	}

	mesh2->priv = indata;
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

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, NULL, NULL, 0);

	// Send a large buffer of data, preceded by the length of the data to be received.

	// TODO: first bytes should be sendable using meshlink_channel_send(), however UTCP doesn't like storing data in its buffers before it has seen a SYNACK.
	if(!meshlink_channel_aio_send(mesh1, channel, &size, sizeof size, NULL, NULL)) {
		fprintf(stderr, "meshlink_channel_aio_send(): %s\n", meshlink_strerror(errno));
		return 1;
	}

	if(!meshlink_channel_aio_send(mesh1, channel, outdata, size / 2, foo_aio_cb, NULL)) {
		fprintf(stderr, "meshlink_channel_aio_send(): %s\n", meshlink_strerror(meshlink_errno));
		return 1;
	}

	if(!meshlink_channel_aio_send(mesh1, channel, outdata + size / 2, size - size / 2, foo_aio_cb, NULL)) {
		fprintf(stderr, "meshlink_channel_aio_send(): %s\n", meshlink_strerror(meshlink_errno));
		return 1;
	}

	for(int i = 0; i < 10; i++) {
		sleep(1);
		if(bar_callbacks >= 2)
			break;
	}

	fprintf(stderr, "Callbacks: foo %d, bar %d\n", foo_callbacks, bar_callbacks);

	if(foo_callbacks != 2) {
		fprintf(stderr, "Expected 2 AIO callbacks for foo\n");
		return 1;
	}

	if(bar_callbacks != 2) {
		fprintf(stderr, "Expected 2 AIO callbacks for bar\n");
		return 1;
	}

	if(memcmp(indata, outdata, size)) {
		fprintf(stderr, "Received data does not match transmitted data\n");
		for(size_t i = 0; i < size; i++) {
			if(indata[i] != outdata[i]) {
				fprintf(stderr, "Difference at position %lu: %x != %x\n", (unsigned long)i, indata[i], outdata[i]);
				break;
			}
		}

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
