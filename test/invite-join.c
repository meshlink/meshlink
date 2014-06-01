#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "meshlink.h"

volatile bool baz_reachable = false;

void status_cb(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	if(!strcmp(node->name, "baz"))
		baz_reachable = reachable;
}

int main(int argc, char *argv[]) {
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("invite_join_conf.1", "foo");
	if(!mesh1)
		return 1;

	meshlink_handle_t *mesh2 = meshlink_open("invite_join_conf.2", "bar");
	if(!mesh2)
		return 1;

	// Start the first instance and have it generate an invitation.

	meshlink_set_node_status_cb(mesh1, status_cb);
	
	if(!meshlink_start(mesh1))
		return 1;

	meshlink_add_address(mesh1, "localhost");
	char *url = meshlink_invite(mesh1, "baz");
	if(!url)
		return 1;

	// Have the second instance join the first.

	if(!meshlink_join(mesh2, url))
		return 1;

	free(url);

	if(!meshlink_start(mesh2))
		return 1;

	// Wait for the two to connect.

	for(int i = 0; i < 20; i++) {
		sleep(1);
		if(baz_reachable)
			break;
	}

	if(!baz_reachable)
		return 1;

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);

	return 0;
}
