/*
    test_cases_destroy.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_destroy.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <sys/types.h>
#include <dirent.h>


/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_meshlink_destroy_01(void **state);
static bool test_meshlink_destroy_01(void);
static void test_case_meshlink_destroy_02(void **state);
static bool test_meshlink_destroy_02(void);
static void test_case_meshlink_destroy_03(void **state);
static bool test_meshlink_destroy_03(void);

static black_box_state_t test_case_meshlink_destroy_01_state = {
	.test_case_name = "test_case_meshlink_destroy_01",
};
static black_box_state_t test_case_meshlink_destroy_02_state = {
	.test_case_name = "test_case_meshlink_destroy_02",
};
static black_box_state_t test_case_meshlink_destroy_03_state = {
	.test_case_name = "test_case_meshlink_destroy_03",
};


/* Execute destroy Test Case # 1 - valid case*/
static void test_case_meshlink_destroy_01(void **state) {
	execute_test(test_meshlink_destroy_01, state);
}

/* Test Steps for destroy Test Case # 1 - Valid case
    Test Steps:
    1. Open instance for NUT
    2. Close NUT, and destroy the confbase
    3. Open the same confbase directory

    Expected Result:
    confbase should be deleted
*/
static bool test_meshlink_destroy_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance
	char *confbase = "destroyconf";
	mesh_handle = meshlink_open(confbase, "nut", "node_sim", 1);
	assert(mesh_handle);

	meshlink_close(mesh_handle);

	// Destroying NUT's confbase
	bool result = meshlink_destroy(confbase);
	assert_int_equal(result, true);

	// Verify whether confbase is removed or not
	DIR *dir = opendir(confbase);
	assert_int_equal(dir, NULL);

	return true;
}

/* Execute destroy Test Case # 2 - passing NULL argument to the API */
static void test_case_meshlink_destroy_02(void **state) {
	execute_test(test_meshlink_destroy_02, state);
}

/* Test Steps for destroy Test Case # 2 - Invalid case
    Test Steps:
    1. Just passing NULL as argument to the API

    Expected Result:
    Return false reporting failure
*/
static bool test_meshlink_destroy_02(void) {
	// Passing NULL as an argument to meshlink_destroy
	bool result = meshlink_destroy(NULL);
	assert_int_equal(result, false);

	return true;
}

/* Execute status Test Case # 3 - destroying non existing file */
static void test_case_meshlink_destroy_03(void **state) {
	execute_test(test_meshlink_destroy_03, state);
}
/* Test Steps for destroy Test Case # 3 - Invalid case
    Test Steps:
    1. unlink if there's any such test file
    2. Call API with that file name

    Expected Result:
    Return false reporting failure
*/
static bool test_meshlink_destroy_03(void) {
	bool result = false;

	// Deletes if there is any file named 'non_existing' already
	unlink("non_existing");

	// Passing non-existing file as an argument to meshlink_destroy
	result = meshlink_destroy("non_existing");
	assert_int_equal(result, false);

	return true;
}


int test_meshlink_destroy(void) {
	const struct CMUnitTest blackbox_destroy_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_01, NULL, NULL,
		                (void *)&test_case_meshlink_destroy_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_02, NULL, NULL,
		                (void *)&test_case_meshlink_destroy_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_03, NULL, NULL,
		                (void *)&test_case_meshlink_destroy_03_state)
	};

	total_tests += sizeof(blackbox_destroy_tests) / sizeof(blackbox_destroy_tests[0]);

	return cmocka_run_group_tests(blackbox_destroy_tests, NULL, NULL);
}
