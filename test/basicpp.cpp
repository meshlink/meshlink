#include <cstring>
#include <iostream>
#include <unistd.h>
#include <cerrno>
#include <cassert>
#include <dirent.h>

#include "meshlink++.h"

using namespace std;

int main(void) {
	assert(meshlink::destroy("basicpp_conf"));

	// Open a new meshlink instance.

	{
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
		assert(!mesh.open("basicpp_conf", "bar", "basicpp", DEV_CLASS_BACKBONE));

		// Open it without giving a name.

		assert(mesh.open("basicpp_conf", nullptr, "basicpp", DEV_CLASS_BACKBONE));

		// Check that the name is ignored now, and that we still are "foo".

		self = mesh.get_self();
		assert(self);
		assert(!strcmp(self->name, "foo"));

		// Start and stop the mesh.

		mesh.enable_discovery(false);

		assert(mesh.start());
		mesh.stop();
	}

	// Destroy the mesh.

	assert(meshlink::destroy("basicpp_conf"));

	DIR *dir = opendir("basicpp_conf");
	assert(dir);
	struct dirent *ent;
	while((ent = readdir(dir))) {
		assert(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."));
	}
	closedir(dir);

	return 0;
}
