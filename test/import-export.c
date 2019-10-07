#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "meshlink.h"
#include "utils.h"

struct sync_flag bar_reachable;

void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(reachable && !strcmp(node->name, "bar")) {
		set_sync_flag(&bar_reachable, true);
	}
}

int main() {
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

	meshlink_add_address(mesh1, "localhost");
	meshlink_add_address(mesh2, "localhost");

	char *data = meshlink_export(mesh1);
	assert(data);

	assert(meshlink_import(mesh2, data));
	free(data);

	data = meshlink_export(mesh2);
	assert(data);

	assert(meshlink_import(mesh1, data));
	free(data);

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

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);
}
