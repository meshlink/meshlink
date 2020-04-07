/*
    test_cases_invite.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>

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

#ifdef NDEBUG
#undef NDEBUG
#endif

#include "execute_tests.h"
#include "test_cases_invite.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "../../utils.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <signal.h>
#include <linux/limits.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

#define NUT                         "nut"
#define PEER                        "peer"
#define TEST_MESHLINK_INVITE        "test_invite"
#define create_path(confbase, node_name, test_case_no)   assert(snprintf(confbase, sizeof(confbase), TEST_MESHLINK_INVITE "_%ld_%s_%02d", (long) getpid(), node_name, test_case_no) > 0)

static void test_case_invite_01(void **state);
static bool test_invite_01(void);
static void test_case_invite_02(void **state);
static bool test_invite_02(void);
static void test_case_invite_03(void **state);
static bool test_invite_03(void);
static void test_case_invite_04(void **state);
static bool test_invite_04(void);
static void test_case_invite_05(void **state);
static bool test_invite_05(void);

/* State structure for invite API Test Case #1 */
static black_box_state_t test_case_invite_01_state = {
	.test_case_name = "test_case_invite_01",
};

/* State structure for invite API Test Case #2 */
static black_box_state_t test_case_invite_02_state = {
	.test_case_name = "test_case_invite_02",
};

/* State structure for invite API Test Case #3 */
static black_box_state_t test_case_invite_03_state = {
	.test_case_name = "test_case_invite_03",
};

/* State structure for invite API Test Case #4 */
static black_box_state_t test_case_invite_04_state = {
	.test_case_name = "test_case_invite_04",
};

/* State structure for invite API Test Case #5 */
static black_box_state_t test_case_invite_05_state = {
	.test_case_name = "test_case_invite_05",
};

/* Execute invite Test Case # 1 - valid case*/
static void test_case_invite_01(void **state) {
	execute_test(test_invite_01, state);
}
/*Test Steps for meshlink_invite Test Case # 1 - Valid case
    Test Steps:
    1. Run NUT
    2. Invite 'new' node

    Expected Result:
    Generates an invitation
*/
static bool test_invite_01(void) {
	char nut_confbase[PATH_MAX];
	char peer_invitation[1000];
	create_path(nut_confbase, NUT, 1);

	// Create meshlink instance

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_INVITE, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	char *invitation = meshlink_invite(mesh, NULL, "new");
	assert_non_null(invitation);

	free(invitation);
	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	return true;
}

/* Execute invite Test Case # 2 - Invalid case*/
static void test_case_invite_02(void **state) {
	execute_test(test_invite_02, state);
}
/*Test Steps for meshlink_invite Test Case # 2 - Invalid case
    Test Steps:
    1. Calling meshlink_invite API with NULL as mesh handle argument

    Expected Result:
    Reports appropriate error by returning NULL
*/
static bool test_invite_02(void) {
	// Trying to generate INVITATION by passing NULL as mesh link handle
	char *invitation = meshlink_invite(NULL, NULL, "nut");
	assert_int_equal(invitation, NULL);

	return true;
}

/* Execute invite Test Case # 3 - Invalid case*/
static void test_case_invite_03(void **state) {
	execute_test(test_invite_03, state);
}
/*Test Steps for meshlink_invite Test Case # 3 - Invalid case
    Test Steps:
    1. Run NUT
    2. Call meshlink_invite with NULL node name argument

    Expected Result:
    Reports appropriate error by returning NULL
*/
static bool test_invite_03(void) {
	char nut_confbase[PATH_MAX];
	char peer_invitation[1000];
	create_path(nut_confbase, NUT, 3);

	// Create meshlink instance

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_INVITE, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	char *invitation = meshlink_invite(mesh, NULL, NULL);
	assert_int_equal(invitation, NULL);

	free(invitation);
	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	return true;
}

/* Execute invite Test Case # 4 - Functionality test*/
static void test_case_invite_04(void **state) {
	execute_test(test_invite_04, state);
}
/*Test Steps for meshlink_invite Test Case # 4 - Functionality test

    Test Steps:
    1. Create node instance
    2. Add a new address to the mesh and invite a node
    3. Add another new address and invite a node

    Expected Result:
    Newly added address should be there in the invitation.
*/
static bool test_invite_04(void) {
	char nut_confbase[PATH_MAX];
	char peer_invitation[1000];
	create_path(nut_confbase, NUT, 4);

	// Create meshlink instance

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_INVITE, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);

	assert_true(meshlink_add_invitation_address(mesh, "11.11.11.11", "2020"));
	char *invitation = meshlink_invite(mesh, NULL, "foo");
	assert_non_null(strstr(invitation, "11.11.11.11:2020"));
	free(invitation);

	assert_true(meshlink_add_invitation_address(mesh, "fe80::1548:d713:3899:f645", "3030"));
	invitation = meshlink_invite(mesh, NULL, "bar");
	assert_non_null(strstr(invitation, "11.11.11.11:2020"));
	assert_non_null(strstr(invitation, "[fe80::1548:d713:3899:f645]:3030"));
	free(invitation);

	meshlink_close(mesh);
	assert_true(meshlink_destroy(nut_confbase));
	return true;
}

/* Execute invite Test Case # 5 - Synchronization testing */
static void test_case_invite_05(void **state) {
	execute_test(test_invite_05, state);
}

static bool test_invite_05(void) {
	bool status;
	pid_t pid;
	int pid_status;
	int pipefd[2];
	char nut_confbase[PATH_MAX];
	char peer_confbase[PATH_MAX];
	char peer_invitation[1000];
	create_path(nut_confbase, NUT, 5);
	create_path(peer_confbase, PEER, 5);

	assert_int_not_equal(pipe(pipefd), -1);

	// Fork a new process in which NUT opens it's instance and raises SIGINT to terminate.

	pid = fork();
	assert_int_not_equal(pid, -1);

	if(!pid) {
		assert(!close(pipefd[0]));
		meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
		meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_INVITE, DEV_CLASS_STATIONARY);
		assert(mesh);

		char *invitation = meshlink_invite(mesh, NULL, PEER);
		write(pipefd[1], invitation, strlen(invitation) + 1);

		raise(SIGINT);
	}

	// Wait for child exit and verify which signal terminated it

	assert_int_not_equal(waitpid(pid, &pid_status, 0), -1);
	assert_int_equal(WIFSIGNALED(pid_status), true);
	assert_int_equal(WTERMSIG(pid_status), SIGINT);

	assert_int_equal(close(pipefd[1]), 0);
	assert_int_not_equal(read(pipefd[0], peer_invitation, sizeof(peer_invitation)), -1);

	// Reopen the NUT instance in the same test suite

	meshlink_set_log_cb(NULL, MESHLINK_DEBUG, log_cb);
	meshlink_handle_t *mesh = meshlink_open(nut_confbase, NUT, TEST_MESHLINK_INVITE, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	meshlink_handle_t *mesh_peer = meshlink_open(peer_confbase, PEER, TEST_MESHLINK_INVITE, DEV_CLASS_STATIONARY);
	assert_non_null(mesh);
	assert_true(meshlink_start(mesh));
	assert_true(meshlink_join(mesh_peer, peer_invitation));

	// Cleanup

	meshlink_close(mesh);
	meshlink_close(mesh_peer);
	assert_true(meshlink_destroy(nut_confbase));
	assert_true(meshlink_destroy(peer_confbase));
	return true;
}

int test_meshlink_invite(void) {
	const struct CMUnitTest blackbox_invite_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_invite_01, NULL, NULL,
		                (void *)&test_case_invite_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_invite_02, NULL, NULL,
		                (void *)&test_case_invite_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_invite_03, NULL, NULL,
		                (void *)&test_case_invite_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_invite_04, NULL, NULL,
		                (void *)&test_case_invite_04_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_invite_05, NULL, NULL,
		                (void *)&test_case_invite_05_state)
	};

	total_tests += sizeof(blackbox_invite_tests) / sizeof(blackbox_invite_tests[0]);

	return cmocka_run_group_tests(blackbox_invite_tests, NULL, NULL);
}
