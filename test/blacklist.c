#define _GNU_SOURCE

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/time.h>

#include "meshlink.h"
#include "devtools.h"
#include "utils.h"

static struct sync_flag bar_connected;
static struct sync_flag bar_disconnected;
static struct sync_flag baz_connected;

static void foo_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;
	(void)reachable;

	if(!strcmp(node->name, "bar")) {
		if(reachable) {
			set_sync_flag(&bar_connected, true);
		} else {
			set_sync_flag(&bar_disconnected, true);
		}
	}
}

static void baz_status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;
	(void)reachable;

	if(!strcmp(node->name, "bar")) {
		if(reachable) {
			set_sync_flag(&baz_connected, true);
		}
	}
}

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Create three instances.

	const char *name[3] = {"foo", "bar", "baz"};
	meshlink_handle_t *mesh[3];
	char *data[3];

	for(int i = 0; i < 3; i++) {
		char *path = NULL;
		assert(asprintf(&path, "blacklist_conf.%d", i) != -1 && path);

		assert(meshlink_destroy(path));
		mesh[i] = meshlink_open(path, name[i], "trio", DEV_CLASS_BACKBONE);
		assert(mesh[i]);
		free(path);

		assert(meshlink_add_address(mesh[i], "localhost"));

		data[i] = meshlink_export(mesh[i]);
		assert(data[i]);

		// Enable default blacklist on all nodes.
		meshlink_set_default_blacklist(mesh[i], true);
	}

	// The first node knows the two other nodes.

	for(int i = 1; i < 3; i++) {
		assert(meshlink_import(mesh[i], data[0]));
		assert(meshlink_import(mesh[0], data[i]));

		assert(meshlink_get_node(mesh[i], name[0]));
		assert(meshlink_get_node(mesh[0], name[i]));

	}

	for(int i = 0; i < 3; i++) {
		free(data[i]);
	}

	// Second and third node should not know each other yet.

	assert(!meshlink_get_node(mesh[1], name[2]));
	assert(!meshlink_get_node(mesh[2], name[1]));

	// Whitelisting and blacklisting by name should work.

	assert(meshlink_whitelist_by_name(mesh[0], "quux"));
	assert(meshlink_blacklist_by_name(mesh[0], "xyzzy"));

	// Since these nodes now exist we should be able to forget them.

	assert(meshlink_forget_node(mesh[0], meshlink_get_node(mesh[0], "quux")));

	// Start the nodes.

	meshlink_set_node_status_cb(mesh[0], foo_status_cb);
	meshlink_set_node_status_cb(mesh[2], baz_status_cb);

	for(int i = 0; i < 3; i++) {
		assert(meshlink_start(mesh[i]));
	}

	// Wait for them to connect.

	assert(wait_sync_flag(&bar_connected, 5));

	// Blacklist bar

	set_sync_flag(&bar_disconnected, false);
	assert(meshlink_blacklist(mesh[0], meshlink_get_node(mesh[0], name[1])));
	assert(wait_sync_flag(&bar_disconnected, 5));

	// Whitelist bar

	set_sync_flag(&bar_connected, false);
	assert(meshlink_whitelist(mesh[0], meshlink_get_node(mesh[0], name[1])));
	assert(wait_sync_flag(&bar_connected, 15));

	// Bar should not connect to baz

	assert(wait_sync_flag(&baz_connected, 5) == false);

	// But it should know about baz by now

	meshlink_node_t *bar = meshlink_get_node(mesh[2], "bar");
	meshlink_node_t *baz = meshlink_get_node(mesh[1], "baz");
	assert(bar);
	assert(baz);

	// Have bar and baz whitelist each other

	assert(meshlink_whitelist(mesh[1], baz));
	assert(meshlink_whitelist(mesh[2], bar));

	// They should connect to each other

	assert(wait_sync_flag(&baz_connected, 15));

	// Trying to forget an active node should fail.

	assert(!meshlink_forget_node(mesh[1], baz));

	// We need to re-acquire the handle to baz

	baz = meshlink_get_node(mesh[1], "baz");
	assert(baz);

	// Stop the mesh.

	for(int i = 0; i < 3; i++) {
		meshlink_stop(mesh[i]);
	}

	// Forgetting a node should work now.

	assert(meshlink_forget_node(mesh[1], baz));

	// Clean up.

	for(int i = 0; i < 3; i++) {
		meshlink_close(mesh[i]);
	}

	// Check that foo has a config file for xyzzy but not quux
	assert(access("blacklist_conf.0/current/hosts/xyzzy", F_OK) == 0);
	assert(access("blacklist_conf.0/current/hosts/quux", F_OK) != 0 && errno == ENOENT);

	// Check that bar has no config file for baz
	assert(access("blacklist_conf.2/current/hosts/bar", F_OK) == 0);
	assert(access("blacklist_conf.1/current/hosts/baz", F_OK) != 0 && errno == ENOENT);
}
