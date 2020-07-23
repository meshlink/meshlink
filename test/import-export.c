#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "meshlink.h"
#include "utils.h"

static struct sync_flag bar_reachable;

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(reachable && !strcmp(node->name, "bar")) {
		set_sync_flag(&bar_reachable, true);
	}
}

int main(void) {
	init_sync_flag(&bar_reachable);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two new meshlink instance.

	assert(meshlink_destroy("import_export_conf.1"));
	assert(meshlink_destroy("import_export_conf.2"));

	meshlink_handle_t *mesh1 = meshlink_open("import_export_conf.1", "foo", "import-export", DEV_CLASS_BACKBONE);
	assert(mesh1);

	meshlink_handle_t *mesh2 = meshlink_open("import_export_conf.2", "bar", "import-export", DEV_CLASS_BACKBONE);
	assert(mesh2);

	// Disable local discovery

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	assert(meshlink_set_canonical_address(mesh1, meshlink_get_self(mesh1), "localhost", NULL));
	assert(meshlink_set_canonical_address(mesh2, meshlink_get_self(mesh2), "localhost", NULL));

	char *data = meshlink_export(mesh1);
	assert(data);

	assert(meshlink_import(mesh2, data));
	free(data);

	data = meshlink_export(mesh2);
	assert(data);

	assert(meshlink_import(mesh1, data));

	// Check that importing twice is fine
	assert(meshlink_import(mesh1, data));
	free(data);

	// Check that importing garbage is not fine
	assert(!meshlink_import(mesh1, "Garbage\n"));

	// Check that foo knows bar, but that it is not reachable.

	time_t last_reachable;
	time_t last_unreachable;
	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar);
	assert(!meshlink_get_node_reachability(mesh1, bar, &last_reachable, &last_unreachable));
	assert(!last_reachable);
	assert(!last_unreachable);

	// Start both instances

	meshlink_set_node_status_cb(mesh1, status_cb);

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));

	// Wait for the two to connect.

	assert(wait_sync_flag(&bar_reachable, 20));

	// Wait for UDP communication to become possible.

	int pmtu = meshlink_get_pmtu(mesh2, meshlink_get_node(mesh2, "bar"));

	for(int i = 0; i < 10 && !pmtu; i++) {
		sleep(1);
		pmtu = meshlink_get_pmtu(mesh2, meshlink_get_node(mesh2, "bar"));
	}

	assert(pmtu);

	// Check that we now have reachability information

	assert(meshlink_get_node_reachability(mesh1, bar, &last_reachable, &last_unreachable));
	assert(last_reachable);

	// Stop the meshes.

	meshlink_stop(mesh1);
	meshlink_stop(mesh2);

	// Check that bar is no longer reachable

	assert(!meshlink_get_node_reachability(mesh1, bar, &last_reachable, &last_unreachable));
	assert(last_reachable);
	assert(last_unreachable);
	assert(last_reachable <= last_unreachable);

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);
}
