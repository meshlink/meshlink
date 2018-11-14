/*
    test_cases_join.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_join.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_meshlink_join_01(void **state);
static bool test_meshlink_join_01(void);
static void test_case_meshlink_join_02(void **state);
static bool test_meshlink_join_02(void);
static void test_case_meshlink_join_03(void **state);
static bool test_meshlink_join_03(void);
static void test_case_meshlink_join_04(void **state);
static bool test_meshlink_join_04(void);

/* State structure for join Test Case #1 */
static black_box_state_t test_case_join_01_state = {
	.test_case_name = "test_case_join_01",
};

/* State structure for join Test Case #1 */
static black_box_state_t test_case_join_02_state = {
	.test_case_name = "test_case_join_02",
};

/* State structure for join Test Case #1 */
static black_box_state_t test_case_join_03_state = {
	.test_case_name = "test_case_join_03",
};

static bool join_status;

/* status callback */
static void status_callback(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
  (void)mesh;

	if(!strcmp(source->name, "relay")) {
		join_status = reach;
	}
}

/* Execute join Test Case # 1 - valid case*/
static void test_case_meshlink_join_01(void **state) {
	execute_test(test_meshlink_join_01, state);
}

/* Test Steps for meshlink_join Test Case # 1 - Valid case

    Test Steps:
    1. Generate invite in relay container and run 'relay' node
    2. Run NUT
    3. Join NUT with relay using invitation generated.

    Expected Result:
    NUT joins relay using the invitation generated.
*/
static bool test_meshlink_join_01(void) {
	meshlink_destroy("join_conf.1");
	meshlink_destroy("join_conf.2");

	// Create node instances
	meshlink_handle_t *mesh1 = meshlink_open("join_conf.1", "nut", "test", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh2 = meshlink_open("join_conf.2", "relay", "test", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Setting node status callback
	meshlink_set_node_status_cb(mesh1, status_callback);

	// Inviting nut
  meshlink_start(mesh2);
  char *invitation = meshlink_invite(mesh2, "nut");
  assert(invitation);

	// Joining Node-Under-Test with relay
	bool ret = meshlink_join(mesh1, invitation);
	assert_int_equal(ret, true);
	assert(meshlink_start(mesh1));
	sleep(1);

	assert_int_equal(join_status, true);

	free(invitation);
	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("join_conf.1");
	meshlink_destroy("join_conf.2");

	return true;
}

/* Execute join Test Case # 2 - Invalid case*/
static void test_case_meshlink_join_02(void **state) {
	execute_test(test_meshlink_join_02, state);
}

/* Test Steps for meshlink_join Test Case # 2 - Invalid case

    Test Steps:
    1. Call meshlink_join with NULL as mesh handler argument.

    Expected Result:
    report error accordingly when NULL is passed as mesh handle argument
*/
static bool test_meshlink_join_02(void) {
	meshlink_destroy("join_conf.3");

	// Create node instances
	meshlink_handle_t *mesh1 = meshlink_open("join_conf.3", "nut", "test", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	char *invitation = meshlink_invite(mesh1, "nodex");

	/* meshlink_join called with NULL as mesh handle and with valid invitation */
	bool ret = meshlink_join(NULL, invitation);
	assert_int_equal(ret, false);

	free(invitation);
	meshlink_close(mesh1);
	meshlink_destroy("join_conf.3");

	return true;
}

/* Execute join Test Case # 3- Invalid case*/
static void test_case_meshlink_join_03(void **state) {
	execute_test(test_meshlink_join_03, state);
}

/* Test Steps for meshlink_join Test Case # 3 - Invalid case

    Test Steps:
    1. Run NUT
    1. Call meshlink_join with NULL as invitation argument.

    Expected Result:
    Report error accordingly when NULL is passed as invite argument
*/
static bool test_meshlink_join_03(void) {
	meshlink_destroy("joinconf.4");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	mesh_handle = meshlink_open("joinconf.4", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Passing NULL as invitation to join API*/
	bool ret  = meshlink_join(mesh_handle, NULL);
	assert_int_equal(ret, false);

	meshlink_close(mesh_handle);
	meshlink_destroy("joinconf.4");
	return true;
}

int test_meshlink_join(void) {
	const struct CMUnitTest blackbox_join_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_01, NULL, NULL,
		(void *)&test_case_join_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_02, NULL, NULL,
		(void *)&test_case_join_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_03, NULL, NULL,
		(void *)&test_case_join_03_state)
	};
	total_tests += sizeof(blackbox_join_tests) / sizeof(blackbox_join_tests[0]);

	int failed = cmocka_run_group_tests(blackbox_join_tests , NULL , NULL);

	return failed;
}

