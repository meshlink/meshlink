#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>

#include "meshlink.h"
#include "utils.h"

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two ephemeral meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open_ephemeral("foo", "ephemeral", DEV_CLASS_BACKBONE);
	meshlink_handle_t *mesh2 = meshlink_open_ephemeral("bar", "ephemeral", DEV_CLASS_BACKBONE);

	assert(mesh1);
	assert(mesh2);

	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, log_cb);
	meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, log_cb);

	// Exchange data

	char *export1 = meshlink_export(mesh1);
	char *export2 = meshlink_export(mesh2);

	assert(export1);
	assert(export2);

	assert(meshlink_import(mesh1, export2));
	assert(meshlink_import(mesh2, export1));

	free(export1);
	free(export2);

	// Check that they know each other

	assert(meshlink_get_node(mesh1, "bar"));
	assert(meshlink_get_node(mesh2, "foo"));

	// Close the ephemeral instances and reopen them.

	meshlink_close(mesh1);
	meshlink_close(mesh2);

	mesh1 = meshlink_open_ephemeral("foo", "ephemeral", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open_ephemeral("bar", "ephemeral", DEV_CLASS_BACKBONE);

	assert(mesh1);
	assert(mesh2);

	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, log_cb);
	meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, log_cb);

	// Check that the nodes no longer know each other

	assert(!meshlink_get_node(mesh1, "bar"));
	assert(!meshlink_get_node(mesh2, "foo"));

	// That's it.

	meshlink_close(mesh1);
	meshlink_close(mesh2);
}
