#define _GNU_SOURCE

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>

#include "meshlink.h"
#include "devtools.h"
#include "utils.h"

static struct sync_flag received;
static struct sync_flag bar_learned_baz;
static struct sync_flag baz_learned_bar;

static void receive_cb(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
	(void)mesh;
	(void)source;

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		set_sync_flag(&received, true);
	}
}

static void bar_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;
	(void)reachable;

	if(!strcmp(node->name, "baz")) {
		set_sync_flag(&bar_learned_baz, true);
	}
}

static void baz_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;
	(void)reachable;

	if(!strcmp(node->name, "bar")) {
		set_sync_flag(&baz_learned_bar, true);
	}
}

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Create three instances.

	const char *name[3] = {"foo", "bar", "baz"};
	meshlink_handle_t *mesh[3];
	char *data[3];

	for(int i = 0; i < 3; i++) {
		char *path = NULL;
		assert(asprintf(&path, "trio2_conf.%d", i) != -1 && path);

		assert(meshlink_destroy(path));
		mesh[i] = meshlink_open(path, name[i], "trio2", DEV_CLASS_BACKBONE);
		assert(mesh[i]);
		free(path);

		assert(meshlink_add_address(mesh[i], "localhost"));

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

	meshlink_set_node_status_cb(mesh[1], bar_status_cb);
	meshlink_set_node_status_cb(mesh[2], baz_status_cb);

	for(int i = 0; i < 3; i++) {
		free(data[i]);
		assert(meshlink_start(mesh[i]));
	}

	// the nodes should now learn about each other

	assert(wait_sync_flag(&bar_learned_baz, 5));
	assert(wait_sync_flag(&baz_learned_bar, 5));

	// Check that the second and third node autoconnect to each other

	devtool_edge_t *edges = NULL;
	size_t nedges = 0;
	assert_after((edges = devtool_get_all_edges(mesh[1], edges, &nedges), nedges == 3), 15);
	free(edges);

	// Stop the nodes nodes

	for(int i = 0; i < 3; i++) {
		meshlink_stop(mesh[i]);
	}

	// Start just the other two nodes

	for(int i = 1; i < 3; i++) {
		assert(meshlink_start(mesh[i]));
	}

	assert(meshlink_get_node(mesh[1], name[2]));
	assert(meshlink_get_node(mesh[2], name[1]));

	// Communication should still be possible

	meshlink_set_receive_cb(mesh[1], receive_cb);

	for(int i = 0; i < 25; i++) {
		assert(meshlink_send(mesh[2], meshlink_get_node(mesh[2], name[1]), "Hello", 5));

		if(wait_sync_flag(&received, 1)) {
			break;
		}
	}

	assert(wait_sync_flag(&received, 1));

	// Clean up.

	for(int i = 0; i < 3; i++) {
		meshlink_close(mesh[i]);
	}
}
