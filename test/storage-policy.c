#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>

#include "meshlink.h"
#include "utils.h"

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	meshlink_handle_t *mesh1;
	meshlink_handle_t *mesh2;

	// Open two instances

	assert(meshlink_destroy("storage-policy_conf.1"));
	assert(meshlink_destroy("storage-policy_conf.2"));

	mesh1 = meshlink_open("storage-policy_conf.1", "foo", "storage-policy", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open("storage-policy_conf.2", "bar", "storage-policy", DEV_CLASS_BACKBONE);
	assert(mesh1);
	assert(mesh2);
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);
	meshlink_set_storage_policy(mesh1, MESHLINK_STORAGE_DISABLED);
	meshlink_set_storage_policy(mesh2, MESHLINK_STORAGE_DISABLED);

	// Exchange data

	char *export1 = meshlink_export(mesh1);
	char *export2 = meshlink_export(mesh2);

	assert(export1);
	assert(export2);

	assert(meshlink_import(mesh1, export2));
	assert(meshlink_import(mesh2, export1));

	// Check that they know each other

	assert(meshlink_get_node(mesh1, "bar"));
	assert(meshlink_get_node(mesh2, "foo"));

	start_meshlink_pair(mesh1, mesh2);

	// Close the instances and reopen them.

	close_meshlink_pair(mesh1, mesh2);

	mesh1 = meshlink_open("storage-policy_conf.1", "foo", "storage-policy", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open("storage-policy_conf.2", "bar", "storage-policy", DEV_CLASS_BACKBONE);
	assert(mesh1);
	assert(mesh2);
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);
	meshlink_set_storage_policy(mesh1, MESHLINK_STORAGE_KEYS_ONLY);
	meshlink_set_storage_policy(mesh2, MESHLINK_STORAGE_KEYS_ONLY);

	// Check that the nodes no longer know each other

	assert(!meshlink_get_node(mesh1, "bar"));
	assert(!meshlink_get_node(mesh2, "foo"));

	// Exchange data again

	assert(meshlink_import(mesh1, export2));
	assert(meshlink_import(mesh2, export1));

	free(export1);
	free(export2);

	// Close the instances and reopen them.

	close_meshlink_pair(mesh1, mesh2);

	mesh1 = meshlink_open("storage-policy_conf.1", "foo", "storage-policy", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open("storage-policy_conf.2", "bar", "storage-policy", DEV_CLASS_BACKBONE);
	assert(mesh1);
	assert(mesh2);
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);
	meshlink_set_storage_policy(mesh1, MESHLINK_STORAGE_KEYS_ONLY);
	meshlink_set_storage_policy(mesh2, MESHLINK_STORAGE_KEYS_ONLY);

	// Check that the nodes know each other

	assert(meshlink_get_node(mesh1, "bar"));
	assert(meshlink_get_node(mesh2, "foo"));

	// Check that we update reachability

	time_t last_reachable;
	time_t last_unreachable;
	assert(!meshlink_get_node_reachability(mesh1, meshlink_get_node(mesh1, "bar"), &last_reachable, &last_unreachable));
	assert(!last_reachable);
	assert(!last_unreachable);

	start_meshlink_pair(mesh1, mesh2);
	stop_meshlink_pair(mesh1, mesh2);

	assert(!meshlink_get_node_reachability(mesh1, meshlink_get_node(mesh1, "bar"), &last_reachable, &last_unreachable));
	assert(last_reachable);
	assert(last_unreachable);

	// But have not stored it

	close_meshlink_pair(mesh1, mesh2);

	mesh1 = meshlink_open("storage-policy_conf.1", "foo", "storage-policy", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open("storage-policy_conf.2", "bar", "storage-policy", DEV_CLASS_BACKBONE);
	assert(mesh1);
	assert(mesh2);
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);
	meshlink_set_storage_policy(mesh1, MESHLINK_STORAGE_KEYS_ONLY);
	meshlink_set_storage_policy(mesh2, MESHLINK_STORAGE_KEYS_ONLY);

	assert(meshlink_get_node(mesh1, "bar"));
	assert(meshlink_get_node(mesh2, "foo"));

	assert(!meshlink_get_node_reachability(mesh1, meshlink_get_node(mesh1, "bar"), &last_reachable, &last_unreachable));
	assert(!last_reachable);
	assert(!last_unreachable);

	// Check that if we change back to STORAGE_ENABLED right before closing, pending changes are still saved

	start_meshlink_pair(mesh1, mesh2);
	stop_meshlink_pair(mesh1, mesh2);

	meshlink_set_storage_policy(mesh1, MESHLINK_STORAGE_ENABLED);
	meshlink_set_storage_policy(mesh2, MESHLINK_STORAGE_ENABLED);

	close_meshlink_pair(mesh1, mesh2);

	mesh1 = meshlink_open("storage-policy_conf.1", "foo", "storage-policy", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open("storage-policy_conf.2", "bar", "storage-policy", DEV_CLASS_BACKBONE);
	assert(mesh1);
	assert(mesh2);
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	assert(!meshlink_get_node_reachability(mesh1, meshlink_get_node(mesh1, "bar"), &last_reachable, &last_unreachable));
	assert(last_reachable);
	assert(last_unreachable);

	// Start again from scratch, now use invite/join instead of import/export

	close_meshlink_pair(mesh1, mesh2);

	assert(meshlink_destroy("storage-policy_conf.1"));
	assert(meshlink_destroy("storage-policy_conf.2"));

	mesh1 = meshlink_open("storage-policy_conf.1", "foo", "storage-policy", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open("storage-policy_conf.2", "bar", "storage-policy", DEV_CLASS_BACKBONE);
	assert(mesh1);
	assert(mesh2);
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);
	meshlink_set_storage_policy(mesh1, MESHLINK_STORAGE_DISABLED);
	meshlink_set_storage_policy(mesh2, MESHLINK_STORAGE_DISABLED);

	// Check that joining is not possible with storage disabled

	assert(meshlink_set_canonical_address(mesh1, meshlink_get_self(mesh1), "localhost", NULL));
	char *invitation = meshlink_invite(mesh1, NULL, "bar");
	assert(invitation);
	assert(meshlink_start(mesh1));
	assert(!meshlink_join(mesh2, invitation));
	assert(meshlink_errno == MESHLINK_EINVAL);
	meshlink_stop(mesh1);

	// Try again with KEYS_ONLY

	meshlink_set_storage_policy(mesh1, MESHLINK_STORAGE_KEYS_ONLY);
	meshlink_set_storage_policy(mesh2, MESHLINK_STORAGE_KEYS_ONLY);

	assert(meshlink_start(mesh1));
	assert(meshlink_join(mesh2, invitation));
	assert(meshlink_errno == MESHLINK_EINVAL);
	meshlink_stop(mesh1);

	start_meshlink_pair(mesh1, mesh2);

	// Close the instances and reopen them.

	close_meshlink_pair(mesh1, mesh2);

	mesh1 = meshlink_open("storage-policy_conf.1", "foo", "storage-policy", DEV_CLASS_BACKBONE);
	mesh2 = meshlink_open("storage-policy_conf.2", "bar", "storage-policy", DEV_CLASS_BACKBONE);
	assert(mesh1);
	assert(mesh2);
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);
	meshlink_set_storage_policy(mesh1, MESHLINK_STORAGE_KEYS_ONLY);
	meshlink_set_storage_policy(mesh2, MESHLINK_STORAGE_KEYS_ONLY);

	// Check that the nodes know each other

	assert(meshlink_get_node(mesh1, "bar"));
	assert(meshlink_get_node(mesh2, "foo"));

	// Done.

	close_meshlink_pair(mesh1, mesh2);
	free(invitation);
}
