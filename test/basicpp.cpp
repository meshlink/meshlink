#include <cstring>
#include <iostream>

#include "meshlink++.h"

using namespace std;

int main(int argc, char *argv[]) {
	// Open a new meshlink instance.

	meshlink::mesh *mesh = meshlink::open("basicpp_conf", "foo");
	if(!mesh) {
		cerr << "Could not initialize configuration for foo\n";
		return 1;
	}

	// Check that our own node exists.

	meshlink::node *self = mesh->get_node("foo");
	if(!self) {
		cerr << "Foo does not know about itself\n";
		return 1;
	}
	if(strcmp(self->name, "foo")) {
		cerr << "Foo thinks its name is " << self->name << "\n";
		return 1;
	}

	// Start and stop the mesh.

	if(!mesh->start()) {
		cerr << "Foo could not start\n";
		return 1;
	}
	mesh->stop();

	// Make sure we can start and stop the mesh again.

	if(!mesh->start()) {
		cerr << "Foo could not start twice\n";
		return 1;
	}
	mesh->stop();

	// Close the mesh and open it again, now with a different name parameter.

	meshlink::close(mesh);

	// Check that the name is ignored now, and that we still are "foo".

	mesh = meshlink::open("basic_conf", "bar");
	if(!mesh) {
		cerr << "Could not open configuration for foo a second time\n";
		return 1;
	}

	if(mesh->get_node("bar")) {
		cerr << "Foo knows about bar, it shouldn't\n";
		return 1;
	}

	self = mesh->get_node("foo");
	if(!self) {
		cerr << "Foo doesn't know about itself the second time\n";
		return 1;
	}
	if(strcmp(self->name, "foo")) {
		cerr << "Foo thinks its name is " << self->name << " the second time\n";
		return 1;
	}

	// Start and stop the mesh.

	if(!mesh->start()) {
		cerr << "Foo could not start a third time\n";
		return 1;
	}
	mesh->stop();

	// That's it.

	meshlink::close(mesh);

	return 0;
}
