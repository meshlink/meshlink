#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

#include "meshlink.h"

volatile bool baz_reachable = false;

void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	static struct timeval tv0;
	struct timeval tv;

	if(tv0.tv_sec == 0) {
		gettimeofday(&tv0, NULL);
	}

	gettimeofday(&tv, NULL);
	fprintf(stderr, "%u.%.03u ", (unsigned int)(tv.tv_sec - tv0.tv_sec), (unsigned int)tv.tv_usec / 1000);

	if(mesh) {
		fprintf(stderr, "(%s) ", mesh->name);
	}

	fprintf(stderr, "[%d] %s\n", level, text);
}

void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	(void)mesh;

	if(!strcmp(node->name, "baz")) {
		baz_reachable = reachable;
	}
}

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open thee new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("invite_join_conf.1", "foo", "invite-join", DEV_CLASS_BACKBONE);

	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	meshlink_set_log_cb(mesh1, MESHLINK_DEBUG, log_cb);

	meshlink_handle_t *mesh2 = meshlink_open("invite_join_conf.2", "bar", "invite-join", DEV_CLASS_BACKBONE);

	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return 1;
	}

	meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, log_cb);

	meshlink_handle_t *mesh3 = meshlink_open("invite_join_conf.3", "quux", "invite-join", DEV_CLASS_BACKBONE);

	if(!mesh3) {
		fprintf(stderr, "Could not initialize configuration for quux\n");
		return 1;
	}

	meshlink_set_log_cb(mesh3, MESHLINK_DEBUG, log_cb);

	// Disable local discovery.

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);
	meshlink_enable_discovery(mesh3, false);

	// Start the first instance and have it generate invitations.

	meshlink_set_node_status_cb(mesh1, status_cb);

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return 1;
	}

	meshlink_add_address(mesh1, "localhost");
	char *baz_url = meshlink_invite(mesh1, NULL, "baz");

	if(!baz_url) {
		fprintf(stderr, "Foo could not generate an invitation for baz\n");
		return 1;
	}

	char *quux_url = meshlink_invite(mesh1, NULL, "quux");

	if(!quux_url) {
		fprintf(stderr, "Foo could not generate an invitation for quux\n");
		return 1;
	}

	fprintf(stderr, "Invitation URL for baz:  %s\n", baz_url);
	fprintf(stderr, "Invitation URL for quux: %s\n", quux_url);

	// Have the second instance join the first.

	if(!meshlink_join(mesh2, baz_url)) {
		fprintf(stderr, "Baz could not join foo's mesh\n");
		return 1;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Baz could not start\n");
		return 1;
	}

	// Wait for the two to connect.

	for(int i = 0; i < 60; i++) {
		sleep(1);

		if(baz_reachable) {
			break;
		}
	}

	if(!baz_reachable) {
		fprintf(stderr, "Baz not reachable for foo after 20 seconds\n");
		return 1;
	}

	int pmtu = meshlink_get_pmtu(mesh1, meshlink_get_node(mesh1, "baz"));

	for(int i = 0; i < 10 && !pmtu; i++) {
		sleep(1);
		pmtu = meshlink_get_pmtu(mesh1, meshlink_get_node(mesh1, "baz"));
	}

	if(!pmtu) {
		fprintf(stderr, "UDP communication with baz not possible after 10 seconds\n");
		return 1;
	}

	// Check that an invitation cannot be used twice

	if(meshlink_join(mesh3, baz_url)) {
		fprintf(stderr, "Quux could join foo's mesh using an already used invitation\n");
		return 1;
	}

	free(baz_url);

	// Check that nodes cannot join with expired invitations

	meshlink_set_invitation_timeout(mesh1, 0);

	if(meshlink_join(mesh3, quux_url)) {
		fprintf(stderr, "Quux could join foo's mesh using an outdated invitation\n");
		return 1;
	}

	free(quux_url);

	// Check that existing nodes cannot join another mesh

	char *corge_url = meshlink_invite(mesh3, NULL, "corge");

	if(!corge_url) {
		fprintf(stderr, "Quux could not generate an invitation for corge\n");
		return 1;
	}

	fprintf(stderr, "Invitation URL for corge: %s\n", corge_url);

	if(!meshlink_start(mesh3)) {
		fprintf(stderr, "Quux could not start\n");
		return 1;
	}

	meshlink_stop(mesh2);

	if(meshlink_join(mesh2, corge_url)) {
		fprintf(stderr, "Bar could join twice\n");
		return 1;
	}

	free(corge_url);

	// Check that invitations work correctly after changing ports

	meshlink_set_invitation_timeout(mesh1, 86400);
	meshlink_stop(mesh1);
	meshlink_stop(mesh3);

	int oldport = meshlink_get_port(mesh1);
	bool success;

	for(int i = 0; i < 100; i++) {
		success = meshlink_set_port(mesh1, 0x9000 + rand() % 0x1000);
	}

	assert(success);
	int newport = meshlink_get_port(mesh1);
	assert(oldport != newport);

	assert(meshlink_start(mesh1));
	quux_url = meshlink_invite(mesh1, NULL, "quux");
	fprintf(stderr, "Invitation URL for quux: %s\n", quux_url);

	// The old port should not be in the invitation URL

	char portstr[10];
	snprintf(portstr, sizeof(portstr), ":%d", oldport);
	assert(!strstr(quux_url, portstr));

	// The new port should be in the invitation URL

	snprintf(portstr, sizeof(portstr), ":%d", newport);
	assert(strstr(quux_url, portstr));

	// The invitation should work

	assert(meshlink_join(mesh3, quux_url));

	// Clean up.

	meshlink_close(mesh3);
	meshlink_close(mesh2);
	meshlink_close(mesh1);

	return 0;
}
