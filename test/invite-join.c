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

static struct sync_flag baz_reachable;
static struct sync_flag seven_reachable;
static struct sync_flag commits_first_flag;

static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(reachable && !strcmp(node->name, "baz")) {
		set_sync_flag(&baz_reachable, true);
	}

	if(reachable && !strcmp(node->name, "seven")) {
		set_sync_flag(&seven_reachable, true);
	}
}

static void invitee_commits_first_cb(bool inviter_first) {
	// Check that eight has committed foo's host config file, but foo hasn't committed eight's
	assert(access("invite_join_conf.8/current/hosts/foo", F_OK) == 0);
	assert(access("invite_join_conf.1/current/hosts/eight", F_OK) == -1 && errno == ENOENT);
	set_sync_flag(&commits_first_flag, !inviter_first);
}

static void inviter_commits_first_cb(bool inviter_first) {
	// Check that foo has committed nine's host config file, but nine hasn't committed foo's
	assert(access("invite_join_conf.1/current/hosts/nine", F_OK) == 0);
	assert(access("invite_join_conf.9/current/hosts/foo", F_OK) == -1 && errno == ENOENT);
	set_sync_flag(&commits_first_flag, inviter_first);
}

int main(void) {
	init_sync_flag(&baz_reachable);
	init_sync_flag(&seven_reachable);
	init_sync_flag(&commits_first_flag);

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	assert(meshlink_destroy("invite_join_conf.1"));
	assert(meshlink_destroy("invite_join_conf.2"));
	assert(meshlink_destroy("invite_join_conf.3"));
	assert(meshlink_destroy("invite_join_conf.4"));
	assert(meshlink_destroy("invite_join_conf.5"));
	assert(meshlink_destroy("invite_join_conf.6"));
	assert(meshlink_destroy("invite_join_conf.7"));
	assert(meshlink_destroy("invite_join_conf.8"));
	assert(meshlink_destroy("invite_join_conf.9"));

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

	// Check that the second instances cannot join if it is already started

	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));
	meshlink_errno = MESHLINK_OK;
	assert(!meshlink_join(mesh2, baz_url));
	assert(meshlink_errno = MESHLINK_EINVAL);

	// Have the second instance join the first.

	meshlink_stop(mesh2);
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

	assert(meshlink_set_canonical_address(mesh1, meshlink_get_self(mesh1), "localhost", NULL));

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

	// Check inviting nodes into a submesh

	assert(!meshlink_get_node_submesh(mesh1, meshlink_get_self(mesh1)));

	meshlink_handle_t *mesh4 = meshlink_open("invite_join_conf.4", "four", "invite-join", DEV_CLASS_BACKBONE);
	meshlink_handle_t *mesh5 = meshlink_open("invite_join_conf.5", "five", "invite-join", DEV_CLASS_BACKBONE);
	meshlink_handle_t *mesh6 = meshlink_open("invite_join_conf.6", "six", "invite-join", DEV_CLASS_BACKBONE);
	assert(mesh4);
	assert(mesh5);
	assert(mesh6);

	meshlink_enable_discovery(mesh4, false);
	meshlink_enable_discovery(mesh5, false);
	meshlink_enable_discovery(mesh6, false);

	meshlink_submesh_t *submesh1 = meshlink_submesh_open(mesh1, "submesh1");
	meshlink_submesh_t *submesh2 = meshlink_submesh_open(mesh1, "submesh2");
	assert(submesh1);
	assert(submesh2);

	char *four_url = meshlink_invite(mesh1, submesh1, mesh4->name);
	char *five_url = meshlink_invite(mesh1, submesh1, mesh5->name);
	char *six_url = meshlink_invite(mesh1, submesh2, mesh6->name);
	assert(four_url);
	assert(five_url);
	assert(six_url);

	assert(meshlink_join(mesh4, four_url));
	assert(meshlink_join(mesh5, five_url));
	assert(meshlink_join(mesh6, six_url));

	free(four_url);
	free(five_url);
	free(six_url);

	assert(meshlink_start(mesh2));
	assert(meshlink_start(mesh4));
	assert(meshlink_start(mesh5));
	assert(meshlink_start(mesh6));

	// Check that each node knows in which submesh it is

	meshlink_submesh_t *mesh4_submesh = meshlink_get_node_submesh(mesh4, meshlink_get_self(mesh4));
	meshlink_submesh_t *mesh5_submesh = meshlink_get_node_submesh(mesh4, meshlink_get_self(mesh5));
	meshlink_submesh_t *mesh6_submesh = meshlink_get_node_submesh(mesh6, meshlink_get_self(mesh6));
	assert(mesh4_submesh);
	assert(mesh5_submesh);
	assert(mesh6_submesh);
	assert(!strcmp(mesh4_submesh->name, "submesh1"));
	assert(!strcmp(mesh5_submesh->name, "submesh1"));
	assert(!strcmp(mesh6_submesh->name, "submesh2"));

	// Wait for nodes to connect, and check that foo sees the right submeshes

	sleep(2);
	meshlink_node_t *mesh1_four = meshlink_get_node(mesh1, mesh4->name);
	meshlink_node_t *mesh1_six = meshlink_get_node(mesh1, mesh6->name);
	assert(meshlink_get_node_submesh(mesh1, meshlink_get_self(mesh1)) == NULL);
	assert(meshlink_get_node_submesh(mesh1, mesh1_four) == submesh1);
	assert(meshlink_get_node_submesh(mesh1, mesh1_six) == submesh2);

	// Check that the new invitees still have the right submesh information

	meshlink_node_t *mesh4_four = meshlink_get_node(mesh4, mesh4->name);
	meshlink_node_t *mesh4_five = meshlink_get_node(mesh4, mesh5->name);
	meshlink_node_t *mesh6_six = meshlink_get_node(mesh6, mesh6->name);
	assert(meshlink_get_node_submesh(mesh4, mesh4_four) == mesh4_submesh);
	assert(meshlink_get_node_submesh(mesh4, mesh4_five) == mesh4_submesh);
	assert(meshlink_get_node_submesh(mesh6, mesh6_six) == mesh6_submesh);

	// Check that bar can see all the nodes in submeshes and vice versa

	assert(meshlink_get_node(mesh2, mesh4->name));
	assert(meshlink_get_node(mesh2, mesh5->name));
	assert(meshlink_get_node(mesh2, mesh6->name));
	assert(meshlink_get_node(mesh4, mesh2->name));
	assert(meshlink_get_node(mesh5, mesh2->name));
	assert(meshlink_get_node(mesh6, mesh2->name));

	// Check that four and five can see each other

	assert(meshlink_get_node(mesh4, mesh5->name));
	assert(meshlink_get_node(mesh5, mesh4->name));

	// Check that the nodes in different submeshes cannot see each other

	assert(!meshlink_get_node(mesh4, mesh6->name));
	assert(!meshlink_get_node(mesh5, mesh6->name));
	assert(!meshlink_get_node(mesh6, mesh4->name));
	assert(!meshlink_get_node(mesh6, mesh5->name));

	// Check that bar sees the right submesh information for the nodes in submeshes

	meshlink_submesh_t *mesh2_four_submesh = meshlink_get_node_submesh(mesh2, meshlink_get_node(mesh2, mesh4->name));
	meshlink_submesh_t *mesh2_five_submesh = meshlink_get_node_submesh(mesh2, meshlink_get_node(mesh2, mesh5->name));
	meshlink_submesh_t *mesh2_six_submesh = meshlink_get_node_submesh(mesh2, meshlink_get_node(mesh2, mesh6->name));
	assert(mesh2_four_submesh);
	assert(mesh2_five_submesh);
	assert(mesh2_six_submesh);
	assert(!strcmp(mesh2_four_submesh->name, "submesh1"));
	assert(!strcmp(mesh2_five_submesh->name, "submesh1"));
	assert(!strcmp(mesh2_six_submesh->name, "submesh2"));

	// Test case #2: check invalid parameters

	meshlink_handle_t *mesh7 = meshlink_open("invite_join_conf.7", "seven", "invite-join", DEV_CLASS_BACKBONE);
	assert(mesh7);
	meshlink_enable_discovery(mesh7, false);
	char *seven_url = meshlink_invite(mesh1, NULL, "seven");
	assert(seven_url);

	meshlink_errno = MESHLINK_OK;
	assert(!meshlink_invite(NULL, NULL, "seven"));
	assert(meshlink_errno == MESHLINK_EINVAL);

	meshlink_errno = MESHLINK_OK;
	assert(!meshlink_invite(mesh1, NULL, NULL));
	assert(meshlink_errno == MESHLINK_EINVAL);

	meshlink_errno = MESHLINK_OK;
	assert(!meshlink_invite(mesh1, NULL, ""));
	assert(meshlink_errno == MESHLINK_EINVAL);

	meshlink_errno = MESHLINK_OK;
	assert(!meshlink_join(NULL, seven_url));
	assert(meshlink_errno == MESHLINK_EINVAL);

	meshlink_errno = MESHLINK_OK;
	assert(!meshlink_join(mesh7, NULL));
	assert(meshlink_errno == MESHLINK_EINVAL);

	meshlink_errno = MESHLINK_OK;
	assert(!meshlink_join(mesh7, ""));
	assert(meshlink_errno == MESHLINK_EINVAL);

	// Test case #3 and #4: check persistence of inviter and invitee

	assert(meshlink_join(mesh7, seven_url));
	free(seven_url);
	meshlink_close(mesh1);
	meshlink_stop(mesh2);
	meshlink_stop(mesh3);
	meshlink_stop(mesh4);
	meshlink_stop(mesh5);
	meshlink_stop(mesh6);
	meshlink_close(mesh7);
	mesh1 = meshlink_open("invite_join_conf.1", "foo", "invite-join", DEV_CLASS_BACKBONE);
	mesh7 = meshlink_open("invite_join_conf.7", "seven", "invite-join", DEV_CLASS_BACKBONE);
	assert(mesh1);
	assert(mesh7);
	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh7, false);
	meshlink_set_node_status_cb(mesh1, status_cb);
	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh7));
	assert(wait_sync_flag(&seven_reachable, 5));
	meshlink_stop(mesh7);

	// Test case #6 and #7: check invalid inviter_commits_first combinations

	meshlink_handle_t *mesh8 = meshlink_open("invite_join_conf.8", "eight", "invite-join", DEV_CLASS_BACKBONE);
	assert(mesh8);
	meshlink_enable_discovery(mesh8, false);
	char *eight_url = meshlink_invite(mesh1, NULL, "eight");
	assert(eight_url);
	meshlink_set_inviter_commits_first(mesh1, true);
	meshlink_set_inviter_commits_first(mesh8, false);
	assert(!meshlink_join(mesh8, eight_url));
	free(eight_url);

	eight_url = meshlink_invite(mesh1, NULL, "eight");
	meshlink_set_inviter_commits_first(mesh1, false);
	meshlink_set_inviter_commits_first(mesh8, true);
	assert(!meshlink_join(mesh8, eight_url));
	free(eight_url);

	// Test case #5: test invitee committing first scenario

	eight_url = meshlink_invite(mesh1, NULL, "eight");
	meshlink_set_inviter_commits_first(mesh1, false);
	meshlink_set_inviter_commits_first(mesh8, false);
	devtool_set_inviter_commits_first = invitee_commits_first_cb;
	assert(meshlink_join(mesh8, eight_url));
	free(eight_url);
	assert(wait_sync_flag(&commits_first_flag, 5));

	// Test case #6: test inviter committing first scenario

	meshlink_handle_t *mesh9 = meshlink_open("invite_join_conf.9", "nine", "invite-join", DEV_CLASS_BACKBONE);
	assert(mesh9);
	meshlink_enable_discovery(mesh9, false);
	char *nine_url = meshlink_invite(mesh1, NULL, "nine");
	meshlink_set_inviter_commits_first(mesh1, true);
	meshlink_set_inviter_commits_first(mesh9, true);
	devtool_set_inviter_commits_first = inviter_commits_first_cb;
	reset_sync_flag(&commits_first_flag);
	assert(meshlink_join(mesh9, nine_url));
	free(nine_url);
	assert(wait_sync_flag(&commits_first_flag, 5));

	// Clean up.

	meshlink_close(mesh9);
	meshlink_close(mesh8);
	meshlink_close(mesh7);
	meshlink_close(mesh6);
	meshlink_close(mesh5);
	meshlink_close(mesh4);
	meshlink_close(mesh3);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
}
