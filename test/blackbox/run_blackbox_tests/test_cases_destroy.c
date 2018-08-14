/*
    test_cases_destroy.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

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


/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_meshlink_destroy_01(void **state);
static bool test_meshlink_destroy_01(void);
static void test_case_meshlink_destroy_02(void **state);
static bool test_meshlink_destroy_02(void);
static void test_case_meshlink_destroy_03(void **state);
static bool test_meshlink_destroy_03(void);

static black_box_state_t test_case_meshlink_destroy_01_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
static black_box_state_t test_case_meshlink_destroy_02_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
static black_box_state_t test_case_meshlink_destroy_03_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/* Execute destroy Test Case # 1 - valid case*/
static void test_case_meshlink_destroy_01(void **state) {
    execute_test(test_meshlink_destroy_01, state);
    return;
}
/* Test Steps for destroy Test Case # 1 - Valid case
    Test Steps:
    1. Run NUT, sleep for a second
    2. Stop and Close NUT, and destroy the confbase

    Expected Result:
    confbase should be deleted
*/
/* TODO: Can meshlink_destroy be implemented using mesh_handle as argument rather
    confbase directly as an argument which can probably be safer */
static bool test_meshlink_destroy_01(void) {
  bool result = false;
  fprintf(stderr, "[ destroy 01 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  mesh_handle = meshlink_open("destroyconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  PRINT_TEST_CASE_MSG("Destroying NUT's confbase\n");
  result = meshlink_destroy("destroyconf");
  if(result) {
    PRINT_TEST_CASE_MSG("destroyed confbase successfully\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("failed to destroy confbase\n");
    return false;
  }
}

/* Execute destroy Test Case # 2 - passing NULL argument to the API */
static void test_case_meshlink_destroy_02(void **state) {
    execute_test(test_meshlink_destroy_02, state);
    return;
}
/* Test Steps for destroy Test Case # 2 - Invalid case
    Test Steps:
    1. Just passing NULL as argument to the API

    Expected Result:
    Return false reporting failure
*/
static bool test_meshlink_destroy_02(void) {
  PRINT_TEST_CASE_MSG("Passing NULL as an argument to meshlink_destroy\n");

  bool result = meshlink_destroy(NULL);

  if (!result) {
    PRINT_TEST_CASE_MSG("Error reported by returning false when NULL is passed as confbase argument\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("Failed to report error when NULL is passed as confbase argument\n");
    return true;
  }
}

/* Execute status Test Case # 3 - destroying non existing file */
static void test_case_meshlink_destroy_03(void **state) {
    execute_test(test_meshlink_destroy_03, state);
    return;
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

  unlink("non_existing_file");
  PRINT_TEST_CASE_MSG("Passing non-existing file as an argument to meshlink_destroy\n");
  result = meshlink_destroy("non_existing_file");
  return !result;
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

  return cmocka_run_group_tests(blackbox_destroy_tests ,NULL , NULL);
}
