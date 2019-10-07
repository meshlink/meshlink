#include <cstring>
#include <iostream>
#include <unistd.h>
#include <cerrno>
#include <cassert>

#include "meshlink++.h"

using namespace std;

int main() {
	// Open a new meshlink instance.

	assert(meshlink::destroy("basicpp_conf"));
	meshlink::mesh mesh("basicpp_conf", "foo", "basicpp", DEV_CLASS_BACKBONE);
	assert(mesh.isOpen());

	// Check that our own node exists.

	meshlink::node *self = mesh.get_self();
	assert(self);
	assert(!strcmp(self->name, "foo"));

	// Disable local discovery.

	mesh.enable_discovery(false);

	// Start and stop the mesh.

	assert(mesh.start());
	mesh.stop();

	// Make sure we can start and stop the mesh again.

	assert(mesh.start());
	mesh.stop();

	// Close the mesh and open it again, now with a different name parameter.

	mesh.close();
	assert(mesh.open("basicpp_conf", "bar", "basicpp", DEV_CLASS_BACKBONE));

	// Check that the name is ignored now, and that we still are "foo".

	assert(!mesh.get_node("bar"));
	self = mesh.get_self();
	assert(self);
	assert(!strcmp(self->name, "foo"));

	// Start and stop the mesh.

	mesh.enable_discovery(false);

	assert(mesh.start());
	mesh.stop();

	assert(meshlink::destroy("basicpp_conf"));
	assert(access("basic.conf", F_OK) == -1 && errno == ENOENT);

	return 0;
}
