#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "meshlink.h"
#include "utils.h"

static struct sync_flag duplicate_detected;

static void handle_duplicate(meshlink_handle_t *mesh, meshlink_node_t *node) {
	set_sync_flag(&duplicate_detected, true);
	assert(meshlink_blacklist(mesh, node));
}

int main(void) {
	init_sync_flag(&duplicate_detected);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open meshlink instances

	static const char *name[4] = {"foo", "bar", "baz", "foo"};
	meshlink_handle_t *mesh[4];

	for(int i = 0; i < 4; i++) {
		char dirname[100];
		snprintf(dirname, sizeof dirname, "duplicate_conf.%d", i);

		assert(meshlink_destroy(dirname));
		mesh[i] = meshlink_open(dirname, name[i], "duplicate", DEV_CLASS_BACKBONE);
		assert(mesh[i]);

		assert(meshlink_set_canonical_address(mesh[i], meshlink_get_self(mesh[i]), "localhost", NULL));
		meshlink_enable_discovery(mesh[i], false);

		meshlink_set_node_duplicate_cb(mesh[i], handle_duplicate);
	}

	// Link them in a chain

	char *data[4];

	for(int i = 0; i < 4; i++) {
		data[i] = meshlink_export(mesh[i]);
		assert(data[i]);
	}

	for(int i = 0; i < 3; i++) {
		assert(meshlink_import(mesh[i], data[i + 1]));
		assert(meshlink_import(mesh[i + 1], data[i]));
	}

	for(int i = 0; i < 4; i++) {
		free(data[i]);
	}

	// Start the meshes

	for(int i = 0; i < 4; i++) {
		assert(meshlink_start(mesh[i]));
	}

	// Wait for the duplicate node to be detected

	assert(wait_sync_flag(&duplicate_detected, 20));

	// Clean up

	for(int i = 0; i < 4; i++) {
		meshlink_close(mesh[i]);
	}
}
