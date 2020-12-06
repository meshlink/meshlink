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
#include <dirent.h>

#include "meshlink.h"
#include "devtools.h"
#include "utils.h"

static struct sync_flag bar_connected;
static struct sync_flag bar_disconnected;
static struct sync_flag bar_blacklisted;
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

static void bar_blacklisted_cb(meshlink_handle_t *mesh, meshlink_node_t *node) {
	(void)mesh;

	if(!strcmp(node->name, "foo")) {
		set_sync_flag(&bar_blacklisted, true);
	}
}

int main(void) {
	init_sync_flag(&bar_connected);
	init_sync_flag(&bar_disconnected);
	init_sync_flag(&bar_blacklisted);
	init_sync_flag(&baz_connected);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Create three instances.

	const char *name[3] = {"foo", "bar", "baz"};
	meshlink_handle_t *mesh[3];
	char *data[3];

	for(int i = 0; i < 3; i++) {
		char *path = NULL;
		assert(asprintf(&path, "blacklist_conf.%d", i) != -1 && path);

		assert(meshlink_destroy(path));
		mesh[i] = meshlink_open(path, name[i], "blacklist", DEV_CLASS_BACKBONE);
		assert(mesh[i]);
		free(path);

		assert(meshlink_set_canonical_address(mesh[i], meshlink_get_self(mesh[i]), "localhost", NULL));

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

	// Check default blacklist status

	assert(!meshlink_get_node_blacklisted(mesh[0], meshlink_get_self(mesh[0])));
	assert(!meshlink_get_node_blacklisted(mesh[0], meshlink_get_node(mesh[0], name[1])));
	assert(meshlink_get_node_blacklisted(mesh[1], meshlink_get_node(mesh[1], name[2])));
	assert(meshlink_get_node_blacklisted(mesh[2], meshlink_get_node(mesh[2], name[1])));

	// Generate an invitation for a node that is about to be blacklisted

	char *invitation = meshlink_invite(mesh[0], NULL, "xyzzy");
	assert(invitation);
	free(invitation);

	// Whitelisting and blacklisting by name should work.

	assert(meshlink_whitelist_by_name(mesh[0], "quux"));
	assert(meshlink_blacklist_by_name(mesh[0], "xyzzy"));
	assert(meshlink_get_node(mesh[0], "quux"));
	assert(!meshlink_get_node_blacklisted(mesh[0], meshlink_get_node(mesh[0], "quux")));
	assert(meshlink_get_node_blacklisted(mesh[0], meshlink_get_node(mesh[0], "xyzzy")));

	meshlink_node_t **nodes = NULL;
	size_t nnodes = 0;
	nodes = meshlink_get_all_nodes_by_blacklisted(mesh[0], true, nodes, &nnodes);
	assert(nnodes == 1);
	assert(!strcmp(nodes[0]->name, "xyzzy"));

	nodes = meshlink_get_all_nodes_by_blacklisted(mesh[0], false, nodes, &nnodes);
	assert(nnodes == 4);
	assert(!strcmp(nodes[0]->name, "bar"));
	assert(!strcmp(nodes[1]->name, "baz"));
	assert(!strcmp(nodes[2]->name, "foo"));
	assert(!strcmp(nodes[3]->name, "quux"));

	free(nodes);

	// Check that blacklisted nodes are not allowed to be invited, and no invitations are left on disk.

	assert(!meshlink_invite(mesh[0], NULL, "xyzzy"));

	DIR *dir = opendir("blacklist_conf.0/current/invitations");
	assert(dir);
	struct dirent *ent;

	while((ent = readdir(dir))) {
		assert(!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."));
	}

	closedir(dir);

	// Since these nodes now exist we should be able to forget them.

	assert(meshlink_forget_node(mesh[0], meshlink_get_node(mesh[0], "quux")));
	assert(meshlink_get_node_blacklisted(mesh[0], meshlink_get_node(mesh[0], "quux"))); // default blacklisted again

	// Start the nodes.

	meshlink_set_node_status_cb(mesh[0], foo_status_cb);
	meshlink_set_node_status_cb(mesh[2], baz_status_cb);

	for(int i = 0; i < 3; i++) {
		assert(meshlink_start(mesh[i]));
	}

	// Wait for them to connect.

	assert(wait_sync_flag(&bar_connected, 5));

	// Blacklist bar

	meshlink_set_blacklisted_cb(mesh[1], bar_blacklisted_cb);

	set_sync_flag(&bar_disconnected, false);
	assert(meshlink_blacklist(mesh[0], meshlink_get_node(mesh[0], name[1])));
	assert(wait_sync_flag(&bar_disconnected, 5));
	assert(meshlink_get_node_blacklisted(mesh[0], meshlink_get_node(mesh[0], name[1])));

	assert(wait_sync_flag(&bar_blacklisted, 10));

	// Whitelist bar

	set_sync_flag(&bar_connected, false);
	assert(meshlink_whitelist(mesh[0], meshlink_get_node(mesh[0], name[1])));
	assert(wait_sync_flag(&bar_connected, 15));
	assert(!meshlink_get_node_blacklisted(mesh[0], meshlink_get_node(mesh[0], name[1])));

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
	assert(!meshlink_get_node_blacklisted(mesh[1], baz));
	assert(!meshlink_get_node_blacklisted(mesh[2], bar));

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

	// Check that we remember xyzzy but not quux after reopening the mesh
	mesh[0] = meshlink_open("blacklist_conf.0", "foo", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh[0]);
	assert(meshlink_get_node(mesh[0], "xyzzy"));
	assert(!meshlink_get_node(mesh[0], "quux"));

	meshlink_close(mesh[0]);
}
