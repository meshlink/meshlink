#include <string.h>

#include "meshlink.h"

int main(int argc, char *argv[]) {
	// Open a new meshlink instance.

	meshlink_handle_t *mesh = meshlink_open("basic_conf", "foo");
	if(!mesh)
		return 1;

	// Check that our own node exists.

	meshlink_node_t *self = meshlink_get_node(mesh, "foo");
	if(!self)
		return 1;
	if(strcmp(self->name, "foo"))
		return 1;

	// Start and stop the mesh.

	if(!meshlink_start(mesh))
		return 1;
	meshlink_stop(mesh);

	// Make sure we can start and stop the mesh again.

	if(!meshlink_start(mesh))
		return 1;
	meshlink_stop(mesh);

	// Close the mesh and open it again, now with a different name parameter.

	meshlink_close(mesh);

	// Check that the name is ignored now, and that we still are "foo".

	mesh = meshlink_open("basic_conf", "bar");
	if(!mesh)
		return 1;

	if(meshlink_get_node(mesh, "bar"))
		return 1;

	self = meshlink_get_node(mesh, "foo");
	if(!self)
		return 1;
	if(strcmp(self->name, "foo"))
		return 1;

	// Start and stop the mesh.

	if(!meshlink_start(mesh))
		return 1;
	meshlink_stop(mesh);

	// That's it.

	meshlink_close(mesh);

	return 0;
}
