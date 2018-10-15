/*
    test_cases_import.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_import.h"
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

static void test_case_import_01(void **state);
static bool test_import_01(void);
static void test_case_import_02(void **state);
static bool test_import_02(void);
static void test_case_import_03(void **state);
static bool test_import_03(void);
static void test_case_import_04(void **state);
static bool test_import_04(void);
static void test_case_import_05(void **state);
static bool test_import_05(void);
static void test_case_import_06(void **state);
static bool test_import_06(void);

/* State structure for import API Test Case #1 */
static black_box_state_t test_case_import_01_state = {
    .test_case_name = "test_case_import_01",
};
/* State structure for import API Test Case #2 */
static black_box_state_t test_case_import_02_state = {
    .test_case_name = "test_case_import_02",
};
/* State structure for import API Test Case #3 */
static black_box_state_t test_case_import_03_state = {
    .test_case_name = "test_case_import_03",
};
/* State structure for import API Test Case #4 */
static black_box_state_t test_case_import_04_state = {
    .test_case_name = "test_case_import_04",
};
/* State structure for import API Test Case #5 */
static black_box_state_t test_case_import_05_state = {
    .test_case_name = "test_case_import_05",
};
/* State structure for import API Test Case #6 */
static black_box_state_t test_case_import_06_state = {
    .test_case_name = "test_case_import_06",
};


/* Execute import Test Case # 1 - valid case*/
static void test_case_import_01(void **state) {
    execute_test(test_import_01, state);
    return;
}
/* Test Steps for meshlink_import Test Case # 1 - Valid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Export and Import mutually

    Expected Result:
    Both the nodes imports successfully
*/
static bool test_import_01(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");

  // Opening NUT and bar nodes
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "test", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "test", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  // Exporting and Importing mutually
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);
	bool imp1 = meshlink_import(mesh1, exp2);
	bool imp2 = meshlink_import(mesh2, exp1);
	if(imp1 && imp2) {
    PRINT_TEST_CASE_MSG("meshlink_import mesh1 & mesh2 imported successfully\n");
	} else {
    fprintf(stderr, "Failed to IMPORT mesh1 & mesh2\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return imp1 && imp2;
}

/* Execute import Test Case # 2 - invalid case*/
static void test_case_import_02(void **state) {
    execute_test(test_import_02, state);
    return;
}
/* Test Steps for meshlink_import Test Case # 2 - Invalid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Passing NULL as mesh handle argument for meshlink_import API

    Expected Result:
    Reports error successfully by returning false
*/
static bool test_import_02(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");

  // Opening NUT and bar nodes
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "test", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "test", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  // Exporting & Importing nodes
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);

	bool imp1 = meshlink_import(NULL, exp2);
	bool imp2 = meshlink_import(mesh2, exp1);
	if((!imp1) && imp2 ) {
    PRINT_TEST_CASE_MSG("meshlink_import mesh1 successfully reported error when NULL mesh handler argument error\n");
	} else {
    PRINT_TEST_CASE_MSG("Failed to report NULL argument error\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return (!imp1) && imp2;
}


/* Execute import Test Case # 3 - invalid case*/
static void test_case_import_03(void **state) {
    execute_test(test_import_03, state);
    return;
}
/* Test Steps for meshlink_import Test Case # 3 - Invalid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Passing NULL as exported data argument for meshlink_import API

    Expected Result:
    Reports error successfully by returning false
*/
static bool test_import_03(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");

  /* Opening NUT and bar nodes */
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Exporting & Importing nodes */
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);

	bool imp1 = meshlink_import(mesh1, NULL);
	bool imp2 = meshlink_import(mesh2, exp1);
	if( (!imp1) && imp2 ) {
    PRINT_TEST_CASE_MSG("meshlink_import mesh1 successfully reported error when NULL is passed as exported data argument\n");
	}
	else {
    PRINT_TEST_CASE_MSG("Failed to report NULL argument error\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return (!imp1) && imp2;
}

/* Execute import Test Case # 4 - invalid case garbage string*/
static void test_case_import_04(void **state) {
    execute_test(test_import_04, state);
    return;
}
/* Test Steps for meshlink_import Test Case # 4 - Invalid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Passing some garbage string(NULL terminated)
        as an argument for meshlink_import API

    Expected Result:
    Reports error successfully by returning false
*/
static bool test_import_04(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");

  // Opening NUT and bar nodes
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  // Exporting & Importing nodes
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);

	// Importing NUT with garbage string as exported data argument
	bool imp1 = meshlink_import(mesh1, "1/2/3");
	bool imp2 = meshlink_import(mesh2, exp1);
	if((!imp1) && imp2 ) {
    PRINT_TEST_CASE_MSG("meshlink_import mesh1 successfully reported error when a garbage string is passed as exported data argument\n");
	} else {
    PRINT_TEST_CASE_MSG("Failed to report error when a garbage string is used for importing meta data\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return (!imp1) && imp2;
}

/* Execute import Test Case # 5 - valid case*/
static void test_case_import_05(void **state) {
    execute_test(test_import_05, state);
    return;
}
/* Test Steps for meshlink_import Test Case # 5 - Invalid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Export and Import mutually
    2. Try to import NUT again/twice at 'bar' node

    Expected Result:
    Reports error successfully by returning false
*/
static bool test_import_05(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");

  /* Opening NUT and bar nodes */
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Exporting  & Importing nodes */
  PRINT_TEST_CASE_MSG("Exporting NUT & bar\n");
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);
	PRINT_TEST_CASE_MSG("Importing NUT & bar\n");
	bool imp1 = meshlink_import(mesh1, exp2);
	assert(imp1);
	bool imp2 = meshlink_import(mesh2, exp1);
	assert(imp2);

	/** Trying to import twice **/
	PRINT_TEST_CASE_MSG("trying to import twice \n");
	bool imp3 = meshlink_import(mesh2, exp1);
	if(imp3) {
    PRINT_TEST_CASE_MSG("meshlink_import when imported twice returned 'true'\n");
	} else {
    PRINT_TEST_CASE_MSG("meshlink_import when imported twice returned 'false'\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return !imp3;
}

int test_meshlink_import(void) {
  const struct CMUnitTest blackbox_import_tests[] = {
      cmocka_unit_test_prestate_setup_teardown(test_case_import_01, NULL, NULL,
            (void *)&test_case_import_01_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_import_02, NULL, NULL,
            (void *)&test_case_import_02_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_import_03, NULL, NULL,
            (void *)&test_case_import_03_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_import_04, NULL, NULL,
            (void *)&test_case_import_04_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_import_05, NULL, NULL,
            (void *)&test_case_import_05_state)
  };
  total_tests += sizeof(blackbox_import_tests) / sizeof(blackbox_import_tests[0]);

  return cmocka_run_group_tests(blackbox_import_tests ,NULL , NULL);
}
