#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <assert.h>
#include <dirent.h>

#include "meshlink.h"
#include "utils.h"

#include "devtools.h"

static bool fail_stage1(int stage) {
	return stage != 1;
}

static bool fail_stage2(int stage) {
	return stage != 2;
}

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open a new meshlink instance.

	assert(meshlink_destroy("encrypted_conf"));
	meshlink_handle_t *mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "right", 5);
	assert(mesh);

	// Close the mesh and open it again, now with a different key.

	meshlink_close(mesh);

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "wrong", 5);
	assert(!mesh);

	// Open it again, now with the right key.

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "right", 5);
	assert(mesh);

	// Change the encryption key.

	assert(meshlink_encrypted_key_rotate(mesh, "newkey", 6));
	meshlink_close(mesh);

	// Check that we can only reopen it with the new key

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "right", 5);
	assert(!mesh);
	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "newkey", 6);
	assert(mesh);

	// Simulate a failed rotation, we should only be able to open it with the old key

	devtool_keyrotate_probe = fail_stage1;
	assert(!meshlink_encrypted_key_rotate(mesh, "newkey2", 7));
	meshlink_close(mesh);
	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "newkey2", 7);
	assert(!mesh);
	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "newkey", 6);
	assert(mesh);

	// Simulate a succesful rotation that was interrupted before cleaning up old files

	devtool_keyrotate_probe = fail_stage2;
	assert(meshlink_encrypted_key_rotate(mesh, "newkey3", 7));
	meshlink_close(mesh);
	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "newkey", 6);
	assert(!mesh);
	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "newkey3", 7);
	assert(mesh);

	// That's it.

	meshlink_close(mesh);

	// Destroy the mesh.

	assert(meshlink_destroy("encrypted_conf"));

	DIR *dir = opendir("encrypted_conf");
	assert(!dir && errno == ENOENT);
}
