#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "meshlink.h"
#include "utils.h"

int main(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open two new meshlink instance.

	meshlink_handle_t *mesh_a, *mesh_b;
	open_meshlink_pair(&mesh_a, &mesh_b, "sign_verify");

	// Verify that a signature made on one node can be verified by its peer.

	static const char testdata1[] = "Test data 1.";
	static const char testdata2[] = "Test data 2.";

	char sig[MESHLINK_SIGLEN * 2];
	size_t siglen = sizeof(sig) * 2;

	assert(meshlink_sign(mesh_a, testdata1, sizeof(testdata1), sig, &siglen));
	assert(siglen == MESHLINK_SIGLEN);

	meshlink_node_t *a = meshlink_get_node(mesh_b, "a");
	assert(a);

	meshlink_node_t *b = meshlink_get_node(mesh_b, "b");
	assert(b);

	assert(meshlink_verify(mesh_b, a, testdata1, sizeof(testdata1), sig, siglen));

	// Check that bad signatures are revoked

	assert(!meshlink_verify(mesh_b, a, testdata1, sizeof(testdata1), sig, siglen / 2));
	assert(!meshlink_verify(mesh_b, a, testdata1, sizeof(testdata1), sig, siglen * 2));
	assert(!meshlink_verify(mesh_b, a, testdata2, sizeof(testdata2), sig, siglen));
	assert(!meshlink_verify(mesh_b, b, testdata1, sizeof(testdata1), sig, siglen));

	// Clean up.

	close_meshlink_pair(mesh_a, mesh_b);
}
