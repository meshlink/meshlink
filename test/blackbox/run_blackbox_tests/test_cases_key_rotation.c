/*
    test_cases_key_rotation.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2019  Guus Sliepen <guus@meshlink.io>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "execute_tests.h"
#include "test_cases_key_rotation.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../../src/devtools.h"
#include "../../utils.h"

static void test_case_key_rotation_01(void **state);
static bool test_key_rotation_01(void);
static void test_case_key_rotation_02(void **state);
static bool test_key_rotation_02(void);
static void test_case_key_rotation_03(void **state);
static bool test_key_rotation_03(void);
static void test_case_key_rotation_04(void **state);
static bool test_key_rotation_04(void);
static void test_case_key_rotation_05(void **state);
static bool test_key_rotation_05(void);

static void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {

	static const char *levelstr[] = {
		[MESHLINK_DEBUG] = "\x1b[34mDEBUG",
		[MESHLINK_INFO] = "\x1b[32mINFO",
		[MESHLINK_WARNING] = "\x1b[33mWARNING",
		[MESHLINK_ERROR] = "\x1b[31mERROR",
		[MESHLINK_CRITICAL] = "\x1b[31mCRITICAL",
	};

	fprintf(stderr, "%s(%s):\x1b[0m %s\n", mesh->name, levelstr[level], text);
}

/* Execute key rotation Test Case # 1 - Sanity test */
static void test_case_key_rotation_01(void **state) {
	execute_test(test_key_rotation_01, state);
}

/* Test Steps for key rotation Test Case # 1

    Test Steps:
    1. Open encrypted node instance, call encrypted rotate API with
        invalid input parameters to the call.

    Expected Result:
    Key rotate should fail when called with invalid parameters.
*/
static bool test_key_rotation_01(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_destroy("encrypted_conf");

	// Open a new meshlink instance.

	meshlink_handle_t *mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "oldkey", 6);
	assert_int_not_equal(mesh, NULL);

	// Pass invalid arguments

	bool keyrotation_status = meshlink_encrypted_key_rotate(mesh, NULL, 5);
	assert_int_equal(keyrotation_status, false);

	keyrotation_status = meshlink_encrypted_key_rotate(NULL, "newkey", 6);
	assert_int_equal(keyrotation_status, false);

	keyrotation_status = meshlink_encrypted_key_rotate(mesh, "newkey", 0);
	assert_int_equal(keyrotation_status, false);

	// Cleanup

	meshlink_close(mesh);
	meshlink_destroy("encrypted_conf");

	return true;
}

/* Execute key rotation Test Case # 2 - Sanity test */
static void test_case_key_rotation_02(void **state) {
	execute_test(test_key_rotation_02, state);
}

/* Test Steps for key rotation Test Case # 2

    Test Steps:
    1. Open encrypted node instance, rotate it's key with a newkey and close the node.
    2. Reopen the encrypted node instance with the newkey

    Expected Result:
    Opening encrypted node instance should succeed when tried to open with newkey that's
    been changed to new by key rotate API.
*/
static bool test_key_rotation_02(void) {
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_destroy("encrypted_conf");

	// Open a new meshlink instance.

	meshlink_handle_t *mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "oldkey", 6);
	assert_int_not_equal(mesh, NULL);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_cb);

	// Set a new port for the mesh

	int port = 0x1000 + (rand() & 0x7fff);
	assert_int_equal(meshlink_set_port(mesh, port), true);

	// Key rotate the encrypted_conf storage with new key

	bool keyrotation_status = meshlink_encrypted_key_rotate(mesh, "newkey", 6);
	assert_int_equal(keyrotation_status, true);

	meshlink_close(mesh);

	// Reopen the meshlink instance with the new key

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "newkey", 6);
	assert_int_not_equal(mesh, NULL);

	// Validate the port number that we changed in the last run.

	assert_int_equal(meshlink_get_port(mesh), port);

	// Cleanup

	meshlink_close(mesh);
	meshlink_destroy("encrypted_conf");

	return true;
}

/* Execute key rotation Test Case # 3 - Sanity test */
static void test_case_key_rotation_03(void **state) {
	execute_test(test_key_rotation_03, state);
}

/* Test Steps for key rotation Test Case # 3

    Test Steps:
    1. Open encrypted node instance, rotate it's key with a newkey and close the node.
    2. Reopen the encrypted node instance with the oldkey

    Expected Result:
    Opening encrypted node instance should fail when tried to open with oldkey that's
    been changed to new by key rotate API.
*/
static bool test_key_rotation_03(void) {
	meshlink_destroy("encrypted_conf");
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open a new meshlink instance.

	meshlink_handle_t *mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "oldkey", 6);
	assert_int_not_equal(mesh, NULL);

	// Key rotate the encrypted_conf storage with new key

	bool keyrotation_status = meshlink_encrypted_key_rotate(mesh, "newkey", 6);
	assert_int_equal(keyrotation_status, true);

	meshlink_close(mesh);

	// Reopen the meshlink instance with the new key

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "oldkey", 6);
	assert_int_equal(mesh, NULL);

	// Cleanup

	meshlink_destroy("encrypted_conf");

	return true;
}

/* Execute key rotation Test Case # 4 - Sanity test */
static void test_case_key_rotation_04(void **state) {
	execute_test(test_key_rotation_04, state);
}

/* Test Steps for key rotation Test Case # 4
    Verify whether key rotation API gracefully handles invitations porting from
    old key to new key.

    Test Steps:
    1. Open foo node instance and generate invitations for peer and bar.
    2. Do key rotation with newkey and verify invitation timestamps post key rotation.
    3. Change timestamp of peer key to expire and Open instances of foo, bar and peer nodes
        and try to join bar and peer node.

    Expected Result:
    Key rotation API should never change the any file status attributes of an invitation file.
*/
static bool test_key_rotation_04(void) {
	meshlink_handle_t *mesh;
	meshlink_handle_t *mesh1;
	meshlink_handle_t *mesh2;
	struct dirent *ent;
	DIR *d;
	char invitation_path_buff[500];
	struct stat temp_stat;
	struct stat peer_stat;
	struct utimbuf timebuf;
	bool join_status;
	char *invitations_directory_path = "encrypted_conf/current/invitations/";

	meshlink_destroy("encrypted_conf");
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	// Open a new meshlink instance.

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "oldkey", 6);
	assert_int_not_equal(mesh, NULL);

	// Generate invitations

	char *invitation1 = meshlink_invite(mesh, NULL, "peer");
	assert_int_not_equal(invitation1, NULL);

	// Read the peer invitation file status structure

	strcpy(invitation_path_buff, invitations_directory_path);
	d = opendir(invitation_path_buff);
	assert(d);

	while((ent = readdir(d)) != NULL) {
		if(ent->d_name[0] == '.') {
			continue;
		}

		strcpy(invitation_path_buff, invitations_directory_path);
		strcat(invitation_path_buff, ent->d_name);
		assert(stat(invitation_path_buff, &temp_stat) != -1);

		if((temp_stat.st_mode & S_IFMT) == S_IFREG) {
			break;
		}
	}

	assert(ent);

	closedir(d);

	char *invitation2 = meshlink_invite(mesh, NULL, "bar");
	assert_int_not_equal(invitation2, NULL);

	// Key rotate the encrypted_conf storage with new key

	bool keyrotation_status = meshlink_encrypted_key_rotate(mesh, "newkey", 6);
	assert_int_equal(keyrotation_status, true);

	meshlink_close(mesh);

	// Compare invitation file timestamps of old key with new key

	assert(stat(invitation_path_buff, &peer_stat) != -1);
	assert_int_equal(peer_stat.st_mtime, temp_stat.st_mtime);

	// Change timestamp for @ peer @ node invitation

	timebuf.actime = peer_stat.st_atime;
	timebuf.modtime = peer_stat.st_mtime - 604805; // > 1 week

	assert(utime(invitation_path_buff, &timebuf) != -1);


	// Reopen the meshlink instance with the new key

	mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "newkey", 6);
	assert_int_not_equal(mesh, NULL);

	mesh1 = meshlink_open("encrypted_conf.1", "peer", "encrypted", DEV_CLASS_BACKBONE);
	assert_int_not_equal(mesh1, NULL);

	mesh2 = meshlink_open("encrypted_conf.2", "bar", "encrypted", DEV_CLASS_BACKBONE);
	assert_int_not_equal(mesh2, NULL);

	assert(meshlink_start(mesh));

	join_status = meshlink_join(mesh1, invitation1);
	assert_int_equal(join_status, false);

	join_status = meshlink_join(mesh2, invitation2);
	assert_int_equal(join_status, true);

	// Cleanup

	free(invitation1);
	free(invitation2);
	meshlink_close(mesh);
	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("encrypted_conf");
	meshlink_destroy("encrypted_conf.1");
	meshlink_destroy("encrypted_conf.2");

	return true;
}

/* Execute key rotation Test Case # 5 - Atomicity test */
static void test_case_key_rotation_05(void **state) {
	execute_test(test_key_rotation_05, state);
}

static int break_stage;

static void nop_stage(int stage) {
	(void)stage;

	return;
}

static void debug_probe(int stage) {

	// Terminate the node at the specified stage (by @ break_stage @ )
	if(stage == break_stage) {
		raise(SIGINT);
	} else if((break_stage < 1) || (break_stage > 3)) {
		fprintf(stderr, "INVALID stage break\n");
		raise(SIGABRT);
	}

	return;
}

/* Test Steps for key rotation Test Case # 5
    Debug all stages of key rotate API and verify it's atomicity

    Test Steps:
    1. Open foo node instance.
    2. In a loop break meshlink node instance at each stage incrementally
        in a fork process
    3. Reopen node instance post termination.

    Expected Result:
    Terminating node instance when meshlink_encrypted_key_rotate function called
    at any stage should give atomic result when reopened.
*/
static bool test_key_rotation_05(void) {
	pid_t pid;
	int status;
	meshlink_handle_t *mesh;
	meshlink_destroy("encrypted_conf");
	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);

	assert(signal(SIGINT, SIG_DFL) != SIG_ERR);
	assert(signal(SIGABRT, SIG_DFL) != SIG_ERR);

	// Set debug_probe callback

	devtool_keyrotate_probe = debug_probe;
	int new_port = 12000;
	int pipefd[2];

	// incrementally debug meshlink_encrypted_key_rotate API atomicity

	for(break_stage = 1; break_stage <= 3; break_stage += 1) {
		fprintf(stderr, "Debugging stage %d\n", break_stage);
		meshlink_destroy("encrypted_conf");

		assert(pipe(pipefd) != -1);

		pid = fork();
		assert(pid != -1);

		if(!pid) {
			close(pipefd[0]);
			mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "oldkey", 6);
			assert(mesh);
			meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_cb);
			meshlink_enable_discovery(mesh, false);

			assert(meshlink_set_port(mesh, new_port));

			char *invitation = meshlink_invite(mesh, NULL, "bar");
			assert(invitation);

			assert(write(pipefd[1], invitation, strlen(invitation) + 1) != -1);

			meshlink_encrypted_key_rotate(mesh, "newkey", 6);
			raise(SIGABRT);
		}

		close(pipefd[1]);

		// Wait for child exit and verify which signal terminated it

		assert(waitpid(pid, &status, 0) != -1);
		assert_int_equal(WIFSIGNALED(status), true);
		assert_int_equal(WTERMSIG(status), SIGINT);

		// Reopen the node with invalid key other than old and new key should fail and should not affect
		// the existing confbase

		fprintf(stderr, "Opening mesh with invalid key\n");
		mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "invalidkey", 9);
		assert_int_equal(mesh, NULL);

		// Reopen the node with the "newkey", if it failed to open with "newkey" then
		// opening with the "oldkey" should succeed

		fprintf(stderr, "Opening mesh with new-key\n");
		mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "newkey", 6);

		if(!mesh) {
			fprintf(stderr, "Opening mesh with new-key failed trying to open with old-key\n");
			mesh = meshlink_open_encrypted("encrypted_conf", "foo", "encrypted", DEV_CLASS_BACKBONE, "oldkey", 6);
			assert_int_not_equal(mesh, NULL);
		}

		meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_cb);
		meshlink_enable_discovery(mesh, false);

		// Verify the newly set port and generated invitation

		int get_port = meshlink_get_port(mesh);
		assert_int_equal(get_port, new_port);

		char invitation[200];
		assert(read(pipefd[0], invitation, sizeof(invitation)) != -1);

		assert(meshlink_start(mesh));

		meshlink_destroy("encrypted_conf.1");

		meshlink_handle_t *mesh2 = meshlink_open("encrypted_conf.1", "bar", "bar", DEV_CLASS_BACKBONE);
		assert(mesh2);

		meshlink_set_log_cb(mesh2, MESHLINK_DEBUG, log_cb);
		meshlink_enable_discovery(mesh2, false);

		assert_int_equal(meshlink_join(mesh2, invitation), true);

		// cleanup

		meshlink_close(mesh);
		meshlink_close(mesh2);

		close(pipefd[0]);
	}

	// Cleanup

	meshlink_destroy("encrypted_conf");
	meshlink_destroy("encrypted_conf.1");
	devtool_keyrotate_probe = nop_stage;
	return true;
}

int test_meshlink_encrypted_key_rotation(void) {
	/* State structures for key rotation Test Cases */
	black_box_state_t test_case_key_rotation_01_state = {
		.test_case_name = "test_case_key_rotation_01",
	};
	black_box_state_t test_case_key_rotation_02_state = {
		.test_case_name = "test_case_key_rotation_02",
	};
	black_box_state_t test_case_key_rotation_03_state = {
		.test_case_name = "test_case_key_rotation_03",
	};
	black_box_state_t test_case_key_rotation_04_state = {
		.test_case_name = "test_case_key_rotation_04",
	};
	black_box_state_t test_case_key_rotation_05_state = {
		.test_case_name = "test_case_key_rotation_05",
	};

	const struct CMUnitTest blackbox_status_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_key_rotation_01, NULL, NULL,
		                (void *)&test_case_key_rotation_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_key_rotation_02, NULL, NULL,
		                (void *)&test_case_key_rotation_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_key_rotation_03, NULL, NULL,
		                (void *)&test_case_key_rotation_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_key_rotation_04, NULL, NULL,
		                (void *)&test_case_key_rotation_04_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_key_rotation_05, NULL, NULL,
		                (void *)&test_case_key_rotation_05_state),
	};
	total_tests += sizeof(blackbox_status_tests) / sizeof(blackbox_status_tests[0]);

	return cmocka_run_group_tests(blackbox_status_tests, NULL, NULL);
}
