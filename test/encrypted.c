#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include "meshlink.h"

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

int main() {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open a new meshlink instance.

	meshlink_handle_t *mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "right", 5);

	if(!mesh) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	// Close the mesh and open it again, now with a different key.

	meshlink_close(mesh);

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "wrong", 5);

	if(mesh) {
		fprintf(stderr, "Could open mesh with the wrong key\n");
		return 1;
	}

	// Open it again, now with the right key.

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "right", 5);

	if(!mesh) {
		fprintf(stderr, "Could not open mesh with the right key\n");
		return 1;
	}

	// That's it.

	meshlink_close(mesh);

	// Destroy the mesh.

	if(!meshlink_destroy("encrypted_conf")) {
		fprintf(stderr, "Could not destroy configuration\n");
		return 1;
	}

	if(!access("encrypted_conf", F_OK) || errno != ENOENT) {
		fprintf(stderr, "Configuration not fully destroyed\n");
		return 1;
	}

	return 0;
}
