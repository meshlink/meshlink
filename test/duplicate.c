#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "meshlink.h"

static volatile bool duplicate_detected;

static void handle_duplicate(meshlink_handle_t *mesh, meshlink_node_t *node) {
	meshlink_node_t *self = meshlink_get_self(mesh);
	fprintf(stderr, "%s: detected duplicate node %s\n", self->name, node->name);
	duplicate_detected = true;
	meshlink_blacklist(mesh, node);
}

int main() {
	// Open meshlink instances

	static const char *name[4] = {"foo", "bar", "baz", "foo"};
	meshlink_handle_t *mesh[4];

	for(int i = 0; i < 4; i++) {
		char dirname[100];
		snprintf(dirname, sizeof dirname, "duplicate_conf.%d", i);

		mesh[i] = meshlink_open(dirname, name[i], "duplicate", DEV_CLASS_BACKBONE);

		if(!mesh[i]) {
			fprintf(stderr, "Could not initialize configuration for node %d\n", i);
			return 1;
		}

		meshlink_add_address(mesh[i], "localhost");
		meshlink_enable_discovery(mesh[i], false);

		meshlink_set_node_duplicate_cb(mesh[i], handle_duplicate);
	}

	// Link them in a chain

	char *data[4];

	for(int i = 0; i < 4; i++) {
		data[i] = meshlink_export(mesh[i]);
	}

	for(int i = 0; i < 3; i++) {
		meshlink_import(mesh[i], data[i + 1]);
		meshlink_import(mesh[i + 1], data[i]);
	}

	for(int i = 0; i < 4; i++) {
		free(data[i]);
	}

	// Start the meshes

	for(int i = 0; i < 4; i++) {
		if(!meshlink_start(mesh[i])) {
			fprintf(stderr, "Could not start mesh %d\n", i);
			return 1;
		}
	}

	// Wait for the duplicate node to be detected

	for(int i = 0; i < 20; i++) {
		sleep(1);

		if(duplicate_detected) {
			break;
		}
	}

	if(!duplicate_detected) {
		fprintf(stderr, "Failed to detect duplicate node after 20 seconds\n");
		return 1;
	}

	// Clean up

	for(int i = 0; i < 4; i++) {
		meshlink_close(mesh[i]);
	}

	return 0;
}
