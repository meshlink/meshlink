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

static struct sync_flag baz_reachable;

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(reachable && !strcmp(node->name, "baz")) {
		set_sync_flag(&baz_reachable, true);
	}
}

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	assert(meshlink_destroy("invite_join_conf.1"));
	assert(meshlink_destroy("invite_join_conf.2"));
	assert(meshlink_destroy("invite_join_conf.3"));

	// Open thee new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("invite_join_conf.1", "foo", "invite-join", DEV_CLASS_BACKBONE);
	assert(mesh1);

	meshlink_handle_t *mesh2 = meshlink_open("invite_join_conf.2", "bar", "invite-join", DEV_CLASS_BACKBONE);
	assert(mesh2);

	meshlink_handle_t *mesh3 = meshlink_open("invite_join_conf.3", "quux", "invite-join", DEV_CLASS_BACKBONE);
	assert(mesh3);

	// Disable local discovery.

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);
	meshlink_enable_discovery(mesh3, false);

	// Have the first instance generate invitations.

	meshlink_set_node_status_cb(mesh1, status_cb);

	assert(meshlink_set_canonical_address(mesh1, meshlink_get_self(mesh1), "localhost", NULL));

	char *baz_url = meshlink_invite(mesh1, NULL, "baz");
	assert(baz_url);

	char *quux_url = meshlink_invite(mesh1, NULL, "quux");
	assert(quux_url);

	// Have the second instance join the first.

	assert(meshlink_start(mesh1));

	assert(meshlink_join(mesh2, baz_url));
	assert(meshlink_start(mesh2));

	// Wait for the two to connect.

	assert(wait_sync_flag(&baz_reachable, 20));

	// Wait for UDP communication to become possible.

	int pmtu = meshlink_get_pmtu(mesh1, meshlink_get_node(mesh1, "baz"));

	for(int i = 0; i < 10 && !pmtu; i++) {
		sleep(1);
		pmtu = meshlink_get_pmtu(mesh1, meshlink_get_node(mesh1, "baz"));
	}

	assert(pmtu);

	// Check that an invitation cannot be used twice

	assert(!meshlink_join(mesh3, baz_url));
	free(baz_url);

	// Check that nodes cannot join with expired invitations

	meshlink_set_invitation_timeout(mesh1, 0);

	assert(!meshlink_join(mesh3, quux_url));
	free(quux_url);

	// Check that existing nodes cannot join another mesh

	char *corge_url = meshlink_invite(mesh3, NULL, "corge");
	assert(corge_url);

	assert(meshlink_start(mesh3));

	meshlink_stop(mesh2);

	assert(!meshlink_join(mesh2, corge_url));
	free(corge_url);

	// Check that invitations work correctly after changing ports

	meshlink_set_invitation_timeout(mesh1, 86400);
	meshlink_stop(mesh1);
	meshlink_stop(mesh3);

	int oldport = meshlink_get_port(mesh1);
	bool success = false;

	for(int i = 0; !success && i < 100; i++) {
		success = meshlink_set_port(mesh1, 0x9000 + rand() % 0x1000);
	}

	assert(success);
	int newport = meshlink_get_port(mesh1);
	assert(oldport != newport);

	assert(meshlink_start(mesh1));
	quux_url = meshlink_invite(mesh1, NULL, "quux");
	assert(quux_url);

	// The old port should not be in the invitation URL

	char portstr[10];
	snprintf(portstr, sizeof(portstr), ":%d", oldport);
	assert(!strstr(quux_url, portstr));

	// The new port should be in the invitation URL

	snprintf(portstr, sizeof(portstr), ":%d", newport);
	assert(strstr(quux_url, portstr));

	// The invitation should work

	assert(meshlink_join(mesh3, quux_url));
	free(quux_url);

	// Check that adding duplicate addresses get removed correctly

	assert(meshlink_add_invitation_address(mesh1, "localhost", portstr + 1));
	corge_url = meshlink_invite(mesh1, NULL, "corge");
	assert(corge_url);
	char *localhost = strstr(corge_url, "localhost");
	assert(localhost);
	assert(!strstr(localhost + 1, "localhost"));
	free(corge_url);

	// Check that resetting and adding multiple, different invitation address works

	meshlink_clear_invitation_addresses(mesh1);
	assert(meshlink_add_invitation_address(mesh1, "1.invalid.", "12345"));
	assert(meshlink_add_invitation_address(mesh1, "2.invalid.", NULL));
	assert(meshlink_add_invitation_address(mesh1, "3.invalid.", NULL));
	assert(meshlink_add_invitation_address(mesh1, "4.invalid.", NULL));
	assert(meshlink_add_invitation_address(mesh1, "5.invalid.", NULL));
	char *grault_url = meshlink_invite(mesh1, NULL, "grault");
	assert(grault_url);
	localhost = strstr(grault_url, "localhost");
	assert(localhost);
	char *invalid1 = strstr(grault_url, "1.invalid.:12345");
	assert(invalid1);
	char *invalid5 = strstr(grault_url, "5.invalid.");
	assert(invalid5);

	// Check that explicitly added invitation addresses come before others, in the order they were specified.

	assert(invalid1 < invalid5);
	assert(invalid5 < localhost);
	free(grault_url);

	// Clean up.

	meshlink_close(mesh3);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
}
