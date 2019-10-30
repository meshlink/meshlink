#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>

#include "meshlink.h"
#include "utils.h"

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open a new meshlink instance.

	assert(meshlink_destroy("basic_conf"));
	meshlink_handle_t *mesh = meshlink_open("basic_conf", "foo", "basic", DEV_CLASS_BACKBONE);
	assert(mesh);

	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_cb);

	// Check that our own node exists.

	meshlink_node_t *self = meshlink_get_self(mesh);
	assert(self);
	assert(!strcmp(self->name, "foo"));

	// Start and stop the mesh.

	assert(meshlink_start(mesh));
	meshlink_stop(mesh);

	// Make sure we can start and stop the mesh again.

	assert(meshlink_start(mesh));
	meshlink_stop(mesh);

	// Close the mesh and open it again, now with a different name parameter.

	meshlink_close(mesh);
	mesh = meshlink_open("basic_conf", "bar", "basic", DEV_CLASS_BACKBONE);
	assert(mesh);

	// Check that the name is ignored now, and that we still are "foo".

	assert(!meshlink_get_node(mesh, "bar"));
	self = meshlink_get_self(mesh);
	assert(self);
	assert(!strcmp(self->name, "foo"));

	// Start and stop the mesh.

	assert(meshlink_start(mesh));
	meshlink_stop(mesh);

	// That's it.

	meshlink_close(mesh);

	// Destroy the mesh.

	assert(meshlink_destroy("basic_conf"));
	assert(access("basic_conf", F_OK) == -1 && errno == ENOENT);
}
