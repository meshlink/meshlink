#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "meshlink.h"

int main() {
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("sign_verify_conf.1", "foo", "sign-verify", DEV_CLASS_BACKBONE);

	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return 1;
	}

	meshlink_handle_t *mesh2 = meshlink_open("sign_verify_conf.2", "bar", "sign-verify", DEV_CLASS_BACKBONE);

	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return 1;
	}

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");
	meshlink_add_address(mesh2, "localhost");

	char *data = meshlink_export(mesh1);

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import data from foo\n");
		return 1;
	}

	free(data);

	data = meshlink_export(mesh2);

	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import data from bar\n");
		return 1;
	}

	free(data);

	// Verify that a signature made on one node can be verified by its peer.

	static const char testdata1[] = "Test data 1.";
	static const char testdata2[] = "Test data 2.";

	char sig[MESHLINK_SIGLEN * 2];
	size_t siglen = sizeof(sig) * 2;

	if(!meshlink_sign(mesh1, testdata1, sizeof(testdata1), sig, &siglen)) {
		fprintf(stderr, "Signing failed\n");
		return 1;
	}

	if(siglen != MESHLINK_SIGLEN) {
		fprintf(stderr, "Signature has unexpected length %zu != %zu\n", siglen, MESHLINK_SIGLEN);
		return 1;
	}

	meshlink_node_t *foo = meshlink_get_node(mesh2, "foo");

	if(!foo) {
		fprintf(stderr, "Bar did not know about node foo\n");
		return 1;
	}

	meshlink_node_t *bar = meshlink_get_node(mesh2, "bar");

	if(!bar) {
		fprintf(stderr, "Bar did not know about node bar\n");
		return 1;
	}

	if(!meshlink_verify(mesh2, foo, testdata1, sizeof(testdata1), sig, siglen)) {
		fprintf(stderr, "False negative verification\n");
		return 1;
	}

	// Check that bad signatures are revoked

	if(meshlink_verify(mesh2, foo, testdata1, sizeof(testdata1), sig, siglen / 2)) {
		fprintf(stderr, "False positive verification with half sized signature\n");
		return 1;
	}

	if(meshlink_verify(mesh2, foo, testdata1, sizeof(testdata1), sig, siglen * 2)) {
		fprintf(stderr, "False positive verification with double sized signature\n");
		return 1;
	}

	if(meshlink_verify(mesh2, foo, testdata2, sizeof(testdata2), sig, siglen)) {
		fprintf(stderr, "False positive verification with wrong data\n");
		return 1;
	}

	if(meshlink_verify(mesh2, bar, testdata1, sizeof(testdata1), sig, siglen)) {
		fprintf(stderr, "False positive verification with wrong signer\n");
		return 1;
	}

	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);

	return 0;
}
