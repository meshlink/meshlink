/*
    test_cases_sign.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_sign.h"
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

static void test_case_sign_01(void **state);
static bool test_sign_01(void);
static void test_case_sign_02(void **state);
static bool test_sign_02(void);
static void test_case_sign_03(void **state);
static bool test_sign_03(void);
static void test_case_sign_04(void **state);
static bool test_sign_04(void);
static void test_case_sign_05(void **state);
static bool test_sign_05(void);
static void test_case_sign_06(void **state);
static bool test_sign_06(void);
static void test_case_sign_07(void **state);
static bool test_sign_07(void);

/* State structure for sign API Test Case #1 */
static black_box_state_t test_case_sign_01_state = {
	.test_case_name = "test_case_sign_01",
};

/* State structure for sign API Test Case #2 */
static black_box_state_t test_case_sign_02_state = {
	.test_case_name = "test_case_sign_02",
};

/* State structure for sign API Test Case #3 */
static black_box_state_t test_case_sign_03_state = {
	.test_case_name = "test_case_sign_03",
};

/* State structure for sign API Test Case #4 */
static black_box_state_t test_case_sign_04_state = {
	.test_case_name = "test_case_sign_04",
};

/* State structure for sign API Test Case #5 */
static black_box_state_t test_case_sign_05_state = {
	.test_case_name = "test_case_sign_05",
};

/* State structure for sign API Test Case #6 */
static black_box_state_t test_case_sign_06_state = {
	.test_case_name = "test_case_sign_06",
};

/* State structure for sign API Test Case #7 */
static black_box_state_t test_case_sign_07_state = {
	.test_case_name = "test_case_sign_07",
};


/* Execute sign_data Test Case # 1 - Valid case - sign a data successfully*/
static void test_case_sign_01(void **state) {
	execute_test(test_sign_01, state);
}

/* Test Steps for meshlink_sign Test Case # 1 - Valid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Sign data

    Expected Result:
    Signs data successfully
*/
static bool test_sign_01(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance
	meshlink_handle_t *mesh_handle = meshlink_open("signconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	assert(meshlink_start(mesh_handle));

	// Signing data

	char *data = "Test";
	char sig[MESHLINK_SIGLEN];
	size_t ssize = MESHLINK_SIGLEN;
	bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);

	// Clean up
	meshlink_close(mesh_handle);
	assert(meshlink_destroy("signconf"));

	return ret;
}

/* Execute sign_data Test Case # 2 - Invalid case - meshlink_sign passing NULL as mesh handle argument*/
static void test_case_sign_02(void **state) {
	execute_test(test_sign_02, state);
	return;
}

/* Test Steps for meshlink_sign Test Case # 2 - invalid case

    Test Steps:
    1. meshlink_sign API called by passing NULL as mesh handle argument

    Expected Result:
    API returns false hinting the error.
*/
static bool test_sign_02(void) {
	char *data = "Test";
	char sig[MESHLINK_SIGLEN];
	size_t ssize = MESHLINK_SIGLEN;

	// Calling meshlink_sign API
	bool ret = meshlink_sign(NULL, data, strlen(data) + 1, sig, &ssize);

	if(!ret) {
		PRINT_TEST_CASE_MSG("meshlink_sign Successfully reported error on passing NULL as mesh_handle arg\n");
		return true;
	}

	PRINT_TEST_CASE_MSG("meshlink_sign FAILED to report error on passing NULL as mesh_handle arg\n");
	return false;
}

/* Execute sign_data Test Case # 3 - Invalid case - meshlink_sign passing data to be signed as NULL */
static void test_case_sign_03(void **state) {
	execute_test(test_sign_03, state);
}

/* Test Steps for meshlink_sign Test Case # 3 - invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. meshlink_sign API called by passing NULL as data argument
        that has to be signed.

    Expected Result:
    API returns false hinting the error.
*/
static bool test_sign_03(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance
	meshlink_handle_t *mesh_handle = meshlink_open("signconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	assert(meshlink_start(mesh_handle));

	// Signing Data
	char *data = "Test";
	char sig[MESHLINK_SIGLEN];
	size_t ssize = MESHLINK_SIGLEN;
	bool ret = meshlink_sign(mesh_handle, NULL, strlen(data) + 1, sig, &ssize);

	// Clean up

	meshlink_close(mesh_handle);
	assert(meshlink_destroy("signconf"));

	if(!ret) {
		PRINT_TEST_CASE_MSG("meshlink_sign Successfully reported error on passing NULL as data arg\n");
		return true;
	} else {
		PRINT_TEST_CASE_MSG("meshlink_sign FAILED to report error on passing NULL as data arg\n");
		return false;
	}
}

/* Execute sign_data Test Case # 4 - Invalid case - meshlink_sign passing 0 as size of data
      to be signed */
static void test_case_sign_04(void **state) {
	execute_test(test_sign_04, state);
}

/* Test Steps for meshlink_sign Test Case # 3 - invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. meshlink_sign API called by passing 0 as size of data to be signed

    Expected Result:
    API returns false hinting the error.
*/
static bool test_sign_04(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance

	meshlink_handle_t *mesh_handle = meshlink_open("signconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	assert(meshlink_start(mesh_handle));

	// Signing data

	char *data = "Test";
	char sig[MESHLINK_SIGLEN];
	size_t ssize = MESHLINK_SIGLEN;
	bool ret = meshlink_sign(mesh_handle, data, 0, sig, &ssize);

	// Clean up

	meshlink_close(mesh_handle);
	assert(meshlink_destroy("signconf"));

	if(!ret) {
		PRINT_TEST_CASE_MSG("meshlink_sign Successfully reported error on passing 0 as size of data arg\n");
		return true;
	}

	PRINT_TEST_CASE_MSG("meshlink_sign FAILED to report error on passing 0 as size of data arg\n");
	return false;
}

/* Execute sign_data Test Case # 5 - Invalid case - meshlink_sign passing NULL as
      signature buffer argument*/
static void test_case_sign_05(void **state) {
	execute_test(test_sign_05, state);
}

/* Test Steps for meshlink_sign Test Case # 5 - invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. meshlink_sign API called by passing NULL for signature buffer argument

    Expected Result:
    API returns false hinting the error.
*/
static bool test_sign_05(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance

	meshlink_handle_t *mesh_handle = meshlink_open("signconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	assert(meshlink_start(mesh_handle));

	// Signing data

	char *data = "Test";
	size_t ssize = MESHLINK_SIGLEN;
	bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, NULL, &ssize);

	// Clean up

	meshlink_close(mesh_handle);
	assert(meshlink_destroy("signconf"));

	if(!ret) {
		PRINT_TEST_CASE_MSG("meshlink_sign Successfully reported error on passing NULL as sign arg\n");
		return true;
	}

	PRINT_TEST_CASE_MSG("meshlink_sign FAILED to report error on passing NULL as sign arg\n");
	return false;
}

/* Execute sign_data Test Case # 6 - Invalid case - meshlink_sign passing NULL for size of
      signature argument */
static void test_case_sign_06(void **state) {
	execute_test(test_sign_06, state);
}

/* Test Steps for meshlink_sign Test Case # 6 - invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. meshlink_sign API called by passing NULL for size of signature buffer argument

    Expected Result:
    API returns false hinting the error.
*/
static bool test_sign_06(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance
	meshlink_handle_t *mesh_handle = meshlink_open("signconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	assert(meshlink_start(mesh_handle));

	// Signing data

	char *data = "Test";
	char sig[MESHLINK_SIGLEN];
	bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, NULL);

	// Clean up

	meshlink_close(mesh_handle);
	assert(meshlink_destroy("signconf"));

	if(!ret) {
		PRINT_TEST_CASE_MSG("meshlink_sign Successfully reported error on passing NULL as signsize arg\n");
		return true;
	}

	PRINT_TEST_CASE_MSG("meshlink_sign FAILED to report error on passing NULL as signsize arg\n");
	return false;
}

/* Execute sign_data Test Case # 7 - Invalid case - meshlink_sign passing size of signature < MESHLINK_SIGLEN*/
static void test_case_sign_07(void **state) {
	execute_test(test_sign_07, state);
}

/* Test Steps for meshlink_sign Test Case # 6 - invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. meshlink_sign API called by passing size of signature < MESHLINK_SIGLEN

    Expected Result:
    API returns false hinting the error.
*/
static bool test_sign_07(void) {
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	// Create meshlink instance

	meshlink_handle_t *mesh_handle = meshlink_open("signconf", "nut", "node_sim", 1);
	assert(mesh_handle);
	meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	assert(meshlink_start(mesh_handle));

	// Signing data

	char *data = "Test";
	char sig[MESHLINK_SIGLEN];
	size_t ssize = 5;  //5 < MESHLINK_SIGLEN
	bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);

	// Cleanup

	meshlink_stop(mesh_handle);
	meshlink_close(mesh_handle);
	assert(meshlink_destroy("signconf"));

	if(!ret) {
		PRINT_TEST_CASE_MSG("meshlink_sign Successfully reported error on passing signsize < MESHLINK_SIGLEN arg\n");
		return true;
	}

	PRINT_TEST_CASE_MSG("meshlink_sign FAILED to report error on passing signsize < MESHLINK_SIGLEN arg\n");
	return false;
}


int test_meshlink_sign(void) {
	const struct CMUnitTest blackbox_sign_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_sign_01, NULL, NULL,
		                (void *)&test_case_sign_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_sign_02, NULL, NULL,
		                (void *)&test_case_sign_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_sign_03, NULL, NULL,
		                (void *)&test_case_sign_03_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_sign_04, NULL, NULL,
		                (void *)&test_case_sign_04_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_sign_05, NULL, NULL,
		                (void *)&test_case_sign_05_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_sign_06, NULL, NULL,
		                (void *)&test_case_sign_06_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_sign_07, NULL, NULL,
		                (void *)&test_case_sign_07_state)
	};
	total_tests += sizeof(blackbox_sign_tests) / sizeof(blackbox_sign_tests[0]);

	return cmocka_run_group_tests(blackbox_sign_tests, NULL, NULL);
}
