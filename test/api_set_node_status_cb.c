#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <string.h>

#include "meshlink.h"
#include "utils.h"

static struct sync_flag a_reachable;
static struct sync_flag b_reachable;

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!reachable) {
		return;
	}

	if(!strcmp(node->name, "a")) {
		set_sync_flag(&a_reachable, true);
	} else if(!strcmp(node->name, "b")) {
		set_sync_flag(&b_reachable, true);
	}
}

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	init_sync_flag(&a_reachable);
	init_sync_flag(&b_reachable);

	meshlink_handle_t *mesh1, *mesh2;
	open_meshlink_pair_ephemeral(&mesh1, &mesh2, "api_set_node_status_cb");

	// Test case #1: check that setting a valid status callback will cause it to be called
	//               when the node itself is started or stopped

	meshlink_set_node_status_cb(mesh1, status_cb);
	assert(meshlink_start(mesh1));
	assert(wait_sync_flag(&a_reachable, 5));

	// Test case #2: check that the status callback will be called when another peer is started

	assert(meshlink_start(mesh2));
	assert(wait_sync_flag(&b_reachable, 5));

	// Test case #3: check that passing a NULL pointer for the mesh returns an error

	meshlink_errno = MESHLINK_OK;
	meshlink_set_node_status_cb(NULL, status_cb);
	assert(meshlink_errno == MESHLINK_EINVAL);

	// Done.

	close_meshlink_pair(mesh1, mesh2);
}
