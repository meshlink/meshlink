#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "meshlink/meshlink.h"

int main(int argc, char *argv[]) {
	// Open a new meshlink instance.

	meshlink_handle_t *mesh = meshlink_open("basic_conf", "foo", "basic", DEV_CLASS_BACKBONE);
	if(!mesh) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	// Check that our own node exists.

	meshlink_node_t *self = meshlink_get_node(mesh, "foo");
	if(!self) {
		fprintf(stderr, "Foo does not know about itself\n");
		return 1;
	}
	if(strcmp(self->name, "foo")) {
		fprintf(stderr, "Foo thinks its name is %s\n", self->name);
		return 1;
	}

	// Start and stop the mesh.

	if(!meshlink_start(mesh)) {
		fprintf(stderr, "Foo could not start\n");
		return 1;
	}
	meshlink_stop(mesh);

	// Make sure we can start and stop the mesh again.

	if(!meshlink_start(mesh)) {
		fprintf(stderr, "Foo could not start twice\n");
		return 1;
	}
	meshlink_stop(mesh);

	// Close the mesh and open it again, now with a different name parameter.

	meshlink_close(mesh);

	// Check that the name is ignored now, and that we still are "foo".

	mesh = meshlink_open("basic_conf", "bar", "basic", DEV_CLASS_BACKBONE);
	if(mesh) {
		fprintf(stderr, "Could reopen configuration using name bar instead of foo\n");
		return 1;
	}

	mesh = meshlink_open("basic_conf", NULL, "basic", DEV_CLASS_BACKBONE);
	if(!mesh) {
		fprintf(stderr, "Could not open configuration for foo a second time\n");
		return 1;
	}

	if(strcmp(mesh->name, "foo")) {
		fprintf(stderr, "Configuration is not for foo\n");
		return 1;
	}

	if(meshlink_get_node(mesh, "bar")) {
		fprintf(stderr, "Foo knows about bar, it shouldn't\n");
		return 1;
	}

	self = meshlink_get_node(mesh, "foo");
	if(!self) {
		fprintf(stderr, "Foo doesn't know about itself the second time\n");
		return 1;
	}
	if(strcmp(self->name, "foo")) {
		fprintf(stderr, "Foo thinks its name is %s the second time\n", self->name);
		return 1;
	}

	// Start and stop the mesh.

	if(!meshlink_start(mesh)) {
		fprintf(stderr, "Foo could not start a third time\n");
		return 1;
	}
	meshlink_stop(mesh);

	// That's it.

	meshlink_close(mesh);

	// Destroy the mesh.

	if(!meshlink_destroy("basic_conf")) {
		fprintf(stderr, "Could not destroy configuration\n");
		return 1;
	}

	if(!access("basic_conf", F_OK) || errno != ENOENT) {
		fprintf(stderr, "Configuration not fully destroyed\n");
		return 1;
	}

	// Check that we cannot open it anymore

	mesh = meshlink_open("basic_conf", NULL, "basic", DEV_CLASS_BACKBONE);
	if(mesh) {
		fprintf(stderr, "Could open non-existing configuration with NULL name\n");
		return 1;
	}

	return 0;
}
