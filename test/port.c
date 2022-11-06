#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>
#include <errno.h>

#include "meshlink.h"
#include "devtools.h"
#include "utils.h"


int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	meshlink_handle_t *mesh1;
	meshlink_handle_t *mesh2;

	// Open two instances

	assert(meshlink_destroy("port_conf.1"));
	assert(meshlink_destroy("port_conf.2"));

	mesh1 = meshlink_open("port_conf.1", "foo", "port", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open("port_conf.2", "bar", "port", DEV_CLASS_BACKBONE);

	assert(mesh1);
	assert(mesh2);

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	int port1 = meshlink_get_port(mesh1);
	int port2 = meshlink_get_port(mesh2);
	assert(port1);
	assert(port2);
	assert(port1 != port2);

	// bar cannot take foo's port if foo is still open
	assert(!meshlink_set_port(mesh2, port1));

	// bar can take foo's port of foo is closed
	meshlink_close(mesh1);

	assert(meshlink_set_port(mesh2, port1));
	assert(meshlink_get_port(mesh2) == port1);

	// foo can open but will now use a different port
	mesh1 = meshlink_open("port_conf.1", "foo", "port", DEV_CLASS_BACKBONE);
	assert(mesh1);
	int port1b = meshlink_get_port(mesh1);
	assert(port1b);
	assert(port1b != port1);

	assert(!meshlink_set_port(mesh1, port1));

	// foo can take over bar's old port
	assert(meshlink_set_port(mesh1, port2));

	meshlink_close(mesh1);
	meshlink_close(mesh2);
}
