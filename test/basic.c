#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>
#include <dirent.h>

#include "meshlink.h"
#include "utils.h"

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open a new meshlink instance.

	assert(meshlink_destroy("basic_conf"));
	meshlink_handle_t *mesh = meshlink_open("basic_conf", "foo", "basic", DEV_CLASS_BACKBONE);
	assert(mesh);

	// Check that we can't open a second instance of the same node.

	meshlink_handle_t *mesh2 = meshlink_open("basic_conf", "foo", "basic", DEV_CLASS_BACKBONE);
	assert(!mesh2);

	// Check that we cannot destroy an instance that is in use.

	assert(!meshlink_destroy("basic_conf"));

	// Check that our own node exists.

	meshlink_node_t *self = meshlink_get_self(mesh);
	assert(self);
	assert(!strcmp(self->name, "foo"));

	// Check that we are reachable.

	assert(meshlink_get_node_reachability(mesh, self, NULL, NULL));

	// Start and stop the mesh.

	assert(meshlink_start(mesh));
	meshlink_stop(mesh);

	// Check that we are still reachable.

	assert(meshlink_get_node_reachability(mesh, self, NULL, NULL));

	// Make sure we can start and stop the mesh again.

	assert(meshlink_start(mesh));
	assert(meshlink_start(mesh));
	meshlink_stop(mesh);
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
	meshlink_close(mesh);

	// Check that messing with the config directory will create a new instance.

	assert(unlink("basic_conf/current/meshlink.conf") == 0);
	mesh = meshlink_open("basic_conf", "bar", "basic", DEV_CLASS_BACKBONE);
	assert(mesh);
	assert(!meshlink_get_node(mesh, "foo"));
	self = meshlink_get_self(mesh);
	assert(self);
	assert(!strcmp(self->name, "bar"));
	assert(access("basic_conf/new", X_OK) == -1 && errno == ENOENT);
	meshlink_close(mesh);

	assert(rename("basic_conf/current", "basic_conf/new") == 0);
	mesh = meshlink_open("basic_conf", "baz", "basic", DEV_CLASS_BACKBONE);
	assert(mesh);
	assert(!meshlink_get_node(mesh, "bar"));
	self = meshlink_get_self(mesh);
	assert(self);
	assert(!strcmp(self->name, "baz"));
	assert(access("basic_conf/new", X_OK) == -1 && errno == ENOENT);
	meshlink_close(mesh);

	// Destroy the mesh.

	assert(meshlink_destroy("basic_conf"));

	// Check that the configuration directory is completely empty.

	DIR *dir = opendir("basic_conf");
	assert(dir);
	struct dirent *ent;

	while((ent = readdir(dir))) {
		assert(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."));
	}

	closedir(dir);

	// Check that we can destroy it again.

	assert(meshlink_destroy("basic_conf"));
}
