#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>

#include "meshlink.h"
#include "devtools.h"
#include "utils.h"

static void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)mesh;

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

static bool received = false;

static void receive_cb(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
	(void)mesh;
	(void)source;

	fprintf(stderr, "RECEIVED SOMETHING\n");

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		received = true;
	}
}

int main() {
	// Create three instances.

	const char *name[3] = {"foo", "bar", "baz"};
	meshlink_handle_t *mesh[3];
	char *data[3];

	for(int i = 0; i < 3; i++) {
		char *path;
		asprintf(&path, "trio_conf.%d", i);
		assert(path);

		mesh[i] = meshlink_open(path, name[i], "trio", DEV_CLASS_BACKBONE);
		assert(mesh[i]);

		data[i] = meshlink_export(mesh[i]);
		assert(data[i]);
	}

	// first node knows the two other nodes

	for(int i = 1; i < 3; i++) {
		assert(meshlink_import(mesh[i], data[0]));
		assert(meshlink_import(mesh[0], data[i]));

		assert(meshlink_get_node(mesh[i], name[0]));
		assert(meshlink_get_node(mesh[0], name[i]));
	}

	// second and third node should not know each other yet

	assert(!meshlink_get_node(mesh[1], name[2]));
	assert(!meshlink_get_node(mesh[2], name[1]));

	// start the nodes

	for(int i = 0; i < 3; i++) {
		meshlink_start(mesh[i]);
	}

	// the nodes should now learn about each other

	assert_after(meshlink_get_node(mesh[1], name[2]), 5);
	assert_after(meshlink_get_node(mesh[2], name[1]), 5);

	// Send a packet, expect it is received

	meshlink_set_receive_cb(mesh[1], receive_cb);
	assert_after((meshlink_send(mesh[2], meshlink_get_node(mesh[2], name[1]), "Hello", 5), received), 15);

	// Check that the second and third node have autoconnected to each other

	devtool_edge_t *edges = NULL;
	size_t nedges = 0;
	assert_after((edges = devtool_get_all_edges(mesh[1], edges, &nedges), nedges == 3), 15);

	// Stop the first node

	meshlink_stop(mesh[0]);
	sleep(1);

	// Communication should still be possible

	assert_after((meshlink_send(mesh[2], meshlink_get_node(mesh[2], name[1]), "Hello", 5), received), 15);

	// Stop the other nodes

	for(int i = 1; i < 3; i++) {
		meshlink_stop(mesh[i]);
	}

	sleep(1);

	// Start just the other two nodes

	meshlink_set_log_cb(mesh[1], MESHLINK_DEBUG, log_cb);

	for(int i = 1; i < 3; i++) {
		meshlink_start(mesh[i]);
	}

	assert(meshlink_get_node(mesh[1], name[2]));
	assert(meshlink_get_node(mesh[2], name[1]));

	// Communication should still be possible

	received = false;
	assert_after((meshlink_send(mesh[2], meshlink_get_node(mesh[2], name[1]), "Hello", 5), received), 25);

	// Clean up.

	for(int i = 0; i < 3; i++) {
		meshlink_close(mesh[i]);
	}
}
