/*
    test_cases_open.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_open.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_mesh_open_01(void **state);
static bool test_steps_mesh_open_01(void);
static void test_case_mesh_open_02(void **state);
static bool test_steps_mesh_open_02(void);
static void test_case_mesh_open_03(void **state);
static bool test_steps_mesh_open_03(void);
static void test_case_mesh_open_04(void **state);
static bool test_steps_mesh_open_04(void);
static void test_case_mesh_open_05(void **state);
static bool test_steps_mesh_open_05(void);

/* State structure for meshlink_open Test Case #1 */
static black_box_state_t test_mesh_open_01_state = {
	.test_case_name = "test_case_mesh_open_01",
};

/* State structure for meshlink_open Test Case #2 */
static black_box_state_t test_mesh_open_02_state = {
	.test_case_name = "test_case_mesh_open_02",
};

/* State structure for meshlink_open Test Case #3 */
static black_box_state_t test_mesh_open_03_state = {
	.test_case_name = "test_case_mesh_open_03",
};

/* State structure for meshlink_open Test Case #4 */
static black_box_state_t test_mesh_open_04_state = {
	.test_case_name = "test_case_mesh_open_04",
};

/* State structure for meshlink_open Test Case #5 */
static black_box_state_t test_mesh_open_05_state = {
	.test_case_name = "test_case_mesh_open_05",
};

/* Execute meshlink_open Test Case # 1*/
static void test_case_mesh_open_01(void **state) {
	execute_test(test_steps_mesh_open_01, state);
}

/* Test Steps for meshlink_open Test Case # 1

    Test Steps:
    1. Open the node instance using meshlink_open

    Expected Result:
    meshlink_open API should successfully return a mesh handle.
*/
static bool test_steps_mesh_open_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open("open_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert_int_not_equal(mesh, NULL);

	meshlink_close(mesh);
	assert(meshlink_destroy("open_conf"));
	return true;
}

/* Execute meshlink_open Test Case # 2*/
static void test_case_mesh_open_02(void **state) {
	execute_test(test_steps_mesh_open_02, state);
}

/* Test Steps for meshlink_open Test Case # 2

    Test Steps:
    1. Open the node instance using meshlink_open with NULL as confbase argument

    Expected Result:
    meshlink_open API should successfully report error by returning NULL pointer
*/
static bool test_steps_mesh_open_02(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open(NULL, "foo", "test", DEV_CLASS_STATIONARY);
	assert_int_equal(mesh, NULL);

	return true;
}

/* Execute meshlink_open Test Case # 3 */
static void test_case_mesh_open_03(void **state) {
	execute_test(test_steps_mesh_open_03, state);
}

/* Test Steps for meshlink_open Test Case # 3

    Test Steps:
    1. Open the node instance using meshlink_open with NULL as node name argument

    Expected Result:
    meshlink_open API should successfully report error by returning NULL pointer
*/
static bool test_steps_mesh_open_03(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open("openconf", NULL, "test", DEV_CLASS_STATIONARY);
	assert_int_equal(mesh, NULL);

	assert(meshlink_destroy("open_conf"));
	return true;
}

/* Execute meshlink_open Test Case # 4*/
static void test_case_mesh_open_04(void **state) {
	execute_test(test_steps_mesh_open_04, state);
}

/* Test Steps for meshlink_open Test Case # 4

    Test Steps:
    1. Open the node instance using meshlink_open with NULL as app name argument

    Expected Result:
    meshlink_open API should successfully report error by returning NULL pointer
*/
static bool test_steps_mesh_open_04(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open("openconf", "foo", NULL, DEV_CLASS_STATIONARY);
	assert_int_equal(mesh, NULL);

	assert(meshlink_destroy("open_conf"));
	return true;
}

/* Execute meshlink_open Test Case # 5*/
static void test_case_mesh_open_05(void **state) {
	execute_test(test_steps_mesh_open_05, state);
}

/* Test Steps for meshlink_open Test Case # 5

    Test Steps:
    1. Open the node instance using meshlink_open with invalid device class argument

    Expected Result:
    meshlink_open API should successfully report error by returning NULL pointer
*/
static bool test_steps_mesh_open_05(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_handle_t *mesh = meshlink_open("openconf", "foo", "test", -1);
	assert_int_equal(mesh, NULL);

	assert(meshlink_destroy("open_conf"));
	return true;
}

int test_meshlink_open(void) {
	const struct CMUnitTest blackbox_open_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_01, NULL, NULL,
		                (void *)&test_mesh_open_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_02, NULL, NULL,
		                (void *)&test_mesh_open_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_03, NULL, NULL,
		                (void *)&test_mesh_open_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_04, NULL, NULL,
		                (void *)&test_mesh_open_04_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_05, NULL, NULL,
		                (void *)&test_mesh_open_05_state)

	};
	total_tests += sizeof(blackbox_open_tests) / sizeof(blackbox_open_tests[0]);

	return cmocka_run_group_tests(blackbox_open_tests, NULL, NULL);
}
