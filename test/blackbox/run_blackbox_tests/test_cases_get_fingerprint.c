/*
    test_cases_get_fingerprint.c -- Execution of specific meshlink black box test cases
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
#include "test_cases.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_get_fingerprint.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_get_fingerprint_cb_01(void **state);
static bool test_get_fingerprint_cb_01(void);
static void test_case_get_fingerprint_cb_02(void **state);
static bool test_get_fingerprint_cb_02(void);
static void test_case_get_fingerprint_cb_03(void **state);
static bool test_get_fingerprint_cb_03(void);

/* State structure for get_fingerprint Test Case #1 */
static black_box_state_t test_case_get_fingerprint_cb_01_state = {
	.test_case_name = "test_case_get_fingerprint_cb_01",
};
/* State structure for get_fingerprint Test Case #2 */
static black_box_state_t test_case_get_fingerprint_cb_02_state = {
	.test_case_name = "test_case_get_fingerprint_cb_02",
};
/* State structure for get_fingerprint Test Case #3 */
static black_box_state_t test_case_get_fingerprint_cb_03_state = {
	.test_case_name = "test_case_get_fingerprint_cb_03",
};

/* Execute get_fingerprint Test Case # 1 - Valid Case of obtaing publickey of NUT */
static void test_case_get_fingerprint_cb_01(void **state) {
	execute_test(test_get_fingerprint_cb_01, state);
}
/* Test Steps for get_fingerprint Test Case # 1 - Valid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Get node handle for ourself(for NUT) and obtain fingerprint

    Expected Result:
    Obtain fingerprint of NUT successfully.
*/
static bool test_get_fingerprint_cb_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("getfingerprintconf", "nut", "test", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	char *fp = meshlink_get_fingerprint(mesh_handle, node);
	assert_int_not_equal(fp, NULL);

	meshlink_close(mesh_handle);
	meshlink_destroy("getfingerprintconf");

	return true;
}

/* Execute get_fingerprint Test Case # 2 - Invalid Case - trying t0 obtain publickey of a node in a
   mesh by passing NULL as mesh handle argument*/
static void test_case_get_fingerprint_cb_02(void **state) {
	execute_test(test_get_fingerprint_cb_02, state);
}

/* Test Steps for get_fingerprint Test Case # 2 - Invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Get node handle for ourself(for NUT)
    3. Obtain fingerprint by passing NULL as mesh handle

    Expected Result:
    Return NULL by reporting error successfully.
*/
static bool test_get_fingerprint_cb_02(void) {
	/* Set up logging for Meshlink */
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	PRINT_TEST_CASE_MSG("Opening NUT\n");
	meshlink_handle_t *mesh_handle = meshlink_open("getfingerprintconf", "nut", "test", 1);
	assert(mesh_handle);

	/* Set up logging for Meshlink with the newly acquired Mesh Handle */
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	/* Getting node handle for itself */
	meshlink_node_t *node = meshlink_get_self(mesh_handle);
	assert(node != NULL);

	/* passing NULL as mesh handle for meshlink_get_fingerprint API */
	char *fp = meshlink_get_fingerprint(NULL, node);
	assert_int_equal(fp, NULL);

	meshlink_close(mesh_handle);
	meshlink_destroy("getfingerprintconf");

	return true;
}

/* Execute get_fingerprint Test Case # 3 - Invalid Case - trying t0 obtain publickey of a node in a
   mesh by passing NULL as node handle argument */
static void test_case_get_fingerprint_cb_03(void **state) {
	execute_test(test_get_fingerprint_cb_03, state);
}
/* Test Steps for get_fingerprint Test Case # 3 - Invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Get node handle for ourself(for NUT)
    3. Obtain fingerprint by passing NULL as node handle

    Expected Result:
    Return NULL by reporting error successfully.
*/
static bool test_get_fingerprint_cb_03(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("getfingerprintconf", "nut", "test", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	char *fp = meshlink_get_fingerprint(mesh_handle, NULL);
	assert_int_equal(fp, NULL);

	meshlink_close(mesh_handle);
	meshlink_destroy("getfingerprintconf");

	return true;
}

int test_meshlink_get_fingerprint(void) {
	const struct CMUnitTest blackbox_get_fingerprint_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_get_fingerprint_cb_01, NULL, NULL,
		                (void *)&test_case_get_fingerprint_cb_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_get_fingerprint_cb_02, NULL, NULL,
		                (void *)&test_case_get_fingerprint_cb_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_get_fingerprint_cb_03, NULL, NULL,
		                (void *)&test_case_get_fingerprint_cb_03_state)
	};

	total_tests += sizeof(blackbox_get_fingerprint_tests) / sizeof(blackbox_get_fingerprint_tests[0]);

	return cmocka_run_group_tests(blackbox_get_fingerprint_tests, NULL, NULL);

}
