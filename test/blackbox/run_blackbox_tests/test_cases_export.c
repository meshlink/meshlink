/*
    test_cases_export.c -- Execution of specific meshlink black box test cases
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
#include "execute_tests.h"
#include "test_cases_export.h"
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

static void test_case_export_01(void **state);
static bool test_export_01(void);
static void test_case_export_02(void **state);
static bool test_export_02(void);

/* State structure for export API Test Case #1 */
static black_box_state_t test_case_export_01_state = {
	.test_case_name = "test_case_export_01",
};
/* State structure for export API Test Case #2 */
static black_box_state_t test_case_export_02_state = {
	.test_case_name = "test_case_export_02",
};


/* Execute export Test Case # 1 - valid case*/
static void test_case_export_01(void **state) {
	execute_test(test_export_01, state);
}
/* Test Steps for export Test Case # 1 - Valid case
    Test Steps:
    1. Run NUT
    2. Export mesh

    Expected Result:
    API returns a NULL terminated string containing meta data of NUT.
*/
static bool test_export_01(void) {
	meshlink_destroy("exportconf");
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	/* Create meshlink instance */
	meshlink_handle_t *mesh_handle = meshlink_open("exportconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	char *expo = meshlink_export(mesh_handle);
	assert_int_not_equal(expo, NULL);

	meshlink_close(mesh_handle);
	meshlink_destroy("exportconf");

	return true;
}

/* Execute export Test Case # 2 - Invalid case*/
static void test_case_export_02(void **state) {
	execute_test(test_export_02, state);
}
/* Test Steps for export Test Case # 2 - Invalid case
    Test Steps:
    1. Run NUT
    2. calling meshlink_export by passing NULL as mesh handle

    Expected Result:
    API returns NULL reporting error when NULL being passed as mesh handle.
*/
static bool test_export_02(void) {
	// Calling export API with NULL as mesh handle
	char *expo = meshlink_export(NULL);
	assert_int_equal(expo, NULL);

	return true;
}


int test_meshlink_export(void) {
	const struct CMUnitTest blackbox_export_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_export_01, NULL, NULL,
		(void *)&test_case_export_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_export_02, NULL, NULL,
		(void *)&test_case_export_02_state)
	};

	total_tests += sizeof(blackbox_export_tests) / sizeof(blackbox_export_tests[0]);

	return cmocka_run_group_tests(blackbox_export_tests, NULL, NULL);
}

