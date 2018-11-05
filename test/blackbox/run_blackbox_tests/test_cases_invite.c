/*
    test_cases_invite.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

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

#include "execute_tests.h"
#include "test_cases_invite.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_invite_01(void **state);
static bool test_invite_01(void);
static void test_case_invite_02(void **state);
static bool test_invite_02(void);
static void test_case_invite_03(void **state);
static bool test_invite_03(void);
static void test_case_invite_04(void **state);
static bool test_invite_04(void);

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

/* Execute invite Test Case # 1 - valid case*/
static void test_case_invite_01(void **state) {
	execute_test(test_invite_01, state);
	return;
}
/*Test Steps for meshlink_invite Test Case # 1 - Valid case
    Test Steps:
    1. Run NUT
    2. Invite 'new' node

    Expected Result:
    Generates an invitation
*/
static bool test_invite_01(void) {
	meshlink_destroy("inviteconf");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance
	meshlink_handle_t *mesh_handle = meshlink_open("inviteconf", "nut", "node_sim", 1);
	fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	char *invitation = meshlink_invite(mesh_handle, "new");

	meshlink_close(mesh_handle);
	meshlink_destroy("inviteconf");

	if(invitation == NULL) {
		PRINT_TEST_CASE_MSG("Failed to generate INVITATION\n");
		return false;
	} else {
		PRINT_TEST_CASE_MSG("Generated INVITATION successfully\t %s \n", invitation);
		return true;
	}
}

/* Execute invite Test Case # 2 - Invalid case*/
static void test_case_invite_02(void **state) {
	execute_test(test_invite_02, state);
	return;
}
/*Test Steps for meshlink_invite Test Case # 2 - Invalid case
    Test Steps:
    1. Calling meshlink_invite API with NULL as mesh handle argument

    Expected Result:
    Reports appropriate error by returning NULL
*/
static bool test_invite_02(void) {
	// Trying to generate INVITATION by passing NULL as mesh link handle
	char *invitation = meshlink_invite(NULL, "nut");
	assert_int_equal(invitation, NULL);

	return true;
}

/* Execute invite Test Case # 3 - Invalid case*/
static void test_case_invite_03(void **state) {
	execute_test(test_invite_03, state);
	return;
}
/*Test Steps for meshlink_invite Test Case # 3 - Invalid case
    Test Steps:
    1. Run NUT
    2. Call meshlink_invite with NULL node name argument

    Expected Result:
    Reports appropriate error by returning NULL
*/
static bool test_invite_03(void) {
	meshlink_destroy("inviteconf");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance
	meshlink_handle_t *mesh_handle = meshlink_open("inviteconf", "nut", "node_sim", 1);
	fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Trying to generate INVITATION by passing NULL as mesh link handle
	char *invitation = meshlink_invite(mesh_handle, NULL);

	meshlink_close(mesh_handle);
	meshlink_destroy("inviteconf");

	if(invitation == NULL) {
		PRINT_TEST_CASE_MSG("invite API reported error SUCCESSFULLY\n");
		return true;
	} else {
		PRINT_TEST_CASE_MSG("Failed to report error\n");
		return false;
	}
}

/* Execute invite Test Case # 4 - Functionality test*/
static void test_case_invite_04(void **state) {
	execute_test(test_invite_04, state);
	return;
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
	meshlink_destroy("inviteconf");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance
	meshlink_handle_t *mesh_handle = meshlink_open("inviteconf", "nut", "test", 1);
	fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	char *hostname1 = "127.1.1.1";
	bool ret = meshlink_add_address(mesh_handle, hostname1);
	char *invitation = meshlink_invite(mesh_handle, "foo");
	assert_int_not_equal(strstr(invitation, hostname1), NULL);

	char *hostname2 = "127.1.2.3";
	ret = meshlink_add_address(mesh_handle, hostname2);
	invitation = meshlink_invite(mesh_handle, "bar");

	// Verify we have both the added addresses
	assert_int_not_equal(strstr(invitation, hostname1), NULL);
	assert_int_not_equal(strstr(invitation, hostname2), NULL);

	meshlink_close(mesh_handle);
	meshlink_destroy("inviteconf");

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
		(void *)&test_case_invite_04_state)
	};

	total_tests += sizeof(blackbox_invite_tests) / sizeof(blackbox_invite_tests[0]);

	return cmocka_run_group_tests(blackbox_invite_tests , NULL , NULL);
}
