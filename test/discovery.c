#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdlib.h>
#include <assert.h>

#include "utils.h"
#include "meshlink.h"

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "discovery");

	// Forget the canoncial address

	assert(meshlink_clear_canonical_address(mesh_a, meshlink_get_node(mesh_a, "b")));
	assert(meshlink_clear_canonical_address(mesh_b, meshlink_get_node(mesh_b, "a")));

	int port_a = meshlink_get_port(mesh_a);
	int port_b = meshlink_get_port(mesh_b);

	// Swap and change ports

	port_a++;
	port_b++;

	meshlink_close(mesh_a);
	assert(meshlink_set_port(mesh_b, port_a));
	meshlink_close(mesh_b);
	mesh_a = meshlink_open("discovery_conf.1", "a", "discovery", DEV_CLASS_BACKBONE);
	assert(mesh_a);
	assert(meshlink_set_port(mesh_a, port_b));
	mesh_b = meshlink_open("discovery_conf.2", "b", "discovery", DEV_CLASS_BACKBONE);
	assert(mesh_b);

	assert(meshlink_get_port(mesh_a) == port_b);
	assert(meshlink_get_port(mesh_b) == port_a);

	// Verify that the nodes can find each other

	meshlink_enable_discovery(mesh_a, true);
	meshlink_enable_discovery(mesh_b, true);

	start_meshlink_pair(mesh_a, mesh_b);

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
}
