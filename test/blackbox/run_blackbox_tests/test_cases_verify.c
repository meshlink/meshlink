/*
    test_cases_verify.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_verify.h"
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

static void test_case_verify_01(void **state);
static bool test_verify_01(void);
static void test_case_verify_02(void **state);
static bool test_verify_02(void);
static void test_case_verify_03(void **state);
static bool test_verify_03(void);
static void test_case_verify_04(void **state);
static bool test_verify_04(void);
static void test_case_verify_05(void **state);
static bool test_verify_05(void);
static void test_case_verify_06(void **state);
static bool test_verify_06(void);
static void test_case_verify_07(void **state);
static bool test_verify_07(void);
static void test_case_verify_08(void **state);
static bool test_verify_08(void);

/* State structure for verify API Test Case #1 */
static black_box_state_t test_case_verify_01_state = {
    /* test_case_name = */ "test_case_verify_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #2 */
static black_box_state_t test_case_verify_02_state = {
    /* test_case_name = */ "test_case_verify_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #3 */
static black_box_state_t test_case_verify_03_state = {
    /* test_case_name = */ "test_case_verify_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #4 */
static black_box_state_t test_case_verify_04_state = {
    /* test_case_name = */ "test_case_verify_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #5 */
static black_box_state_t test_case_verify_05_state = {
    /* test_case_name = */ "test_case_verify_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #6 */
static black_box_state_t test_case_verify_06_state = {
    /* test_case_name = */ "test_case_verify_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};



/* Execute meshlink_verify Test Case # 1 - Valid case - verify a data successfully*/
void test_case_verify_01(void **state) {
  execute_test(test_verify_01, state);
  return;
}

/* Test Steps for meshlink_sign Test Case # 1 - Valid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Sign data with meshlink_sign
    3. Verify data with the sign buffer used while signing

    Expected Result:
    Verifies data successfully with the apt signature
*/
bool test_verify_01(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("verifyconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  char *data = "Test";
  char sig[MESHLINK_SIGLEN];
  size_t ssize = MESHLINK_SIGLEN;
  PRINT_TEST_CASE_MSG("Calling meshlink_sign to sign data\n");
  bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
  assert(ret);
  PRINT_TEST_CASE_MSG("meshlink_sign Successfuly signed data\n");

  PRINT_TEST_CASE_MSG("get nut node_handle\n");
  meshlink_node_t *source = meshlink_get_node(mesh_handle, "nut");
  assert(source != NULL);

  PRINT_TEST_CASE_MSG("Verifying with the signature using meshlink_verify\n");
  ret = meshlink_verify(mesh_handle, source, data, strlen(data) + 1, sig, ssize);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("verifyconf");

  if (!ret) {
    PRINT_TEST_CASE_MSG("meshlink_verify FAILED to verify data\n");
    return false;
  }
  PRINT_TEST_CASE_MSG("meshlink_verify Successfuly verified data\n");
  return true;
}


/* Execute verify_data Test Case # 2 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_02(void **state) {
    execute_test(test_verify_02, state);
    return;
}

/* Test Steps for meshlink_sign Test Case # 2 - Invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Sign data with meshlink_sign
    3. Trying to pass NULL as mesh handle argument
        and other arguments being valid

    Expected Result:
    Reports error accordingly by returning false
*/
bool test_verify_02(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("verifyconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG(" Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  char *data = "Test";
  char sig[MESHLINK_SIGLEN];
  size_t ssize = MESHLINK_SIGLEN;
  PRINT_TEST_CASE_MSG("Calling meshlink_sign to sign data\n");
  bool sret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
  assert(sret);
  PRINT_TEST_CASE_MSG("meshlink_sign Successfuly signed data\n");

  PRINT_TEST_CASE_MSG("get nut node_handle\n");
  meshlink_node_t *source = meshlink_get_node(mesh_handle, "nut");
  assert(source != NULL);

  PRINT_TEST_CASE_MSG("Calling meshlink_verify API passing NULL as mesh handle argument\n");
  bool ret = meshlink_verify(NULL, source, data, strlen(data) + 1, sig, ssize);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("verifyconf");
  if(!ret) {
    PRINT_TEST_CASE_MSG("meshlink_sign Successfuly reported error on passing NULL as mesh_handle arg\n");
    return true;
  }
  PRINT_TEST_CASE_MSG("meshlink_sign FAILED to report error on passing NULL as mesh_handle arg\n");
  return false;
}


/* Execute verify_data Test Case # 3 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_03(void **state) {
    execute_test(test_verify_03, state);
    return;
}

/* Test Steps for meshlink_sign Test Case # 3 - Invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Sign data with meshlink_sign
    3. Trying to pass NULL as source node handle argument
        and other arguments being valid

    Expected Result:
    Reports error accordingly by returning false
*/
bool test_verify_03(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("verifyconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG(" Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  char *data = "Test";
  char sig[MESHLINK_SIGLEN];
  size_t ssize = MESHLINK_SIGLEN;
  PRINT_TEST_CASE_MSG("Calling meshlink_sign to sign data\n");
  bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
  assert(ret);
  PRINT_TEST_CASE_MSG("meshlink_sign Successfuly signed data\n");

  PRINT_TEST_CASE_MSG("Calling meshlink_verify API passing NULL as source node handle argument\n");
  ret = meshlink_verify(mesh_handle, NULL, data, strlen(data) + 1, sig, ssize);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("verifyconf");

  if(!ret) {
    PRINT_TEST_CASE_MSG("meshlink_verify successfully reported NULL as node_handle arg\n");
    return true;
  }
  PRINT_TEST_CASE_MSG("meshlink_verify FAILED to report NULL as node_handle arg\n");
  return false;
}

/* Execute verify_data Test Case # 4 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_04(void **state) {
    execute_test(test_verify_04, state);
    return;
}

/* Test Steps for meshlink_sign Test Case # 4 - Invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Sign data with meshlink_sign
    3. Trying to pass NULL as signed data argument
        and other arguments being valid

    Expected Result:
    Reports error accordingly by returning false
*/
bool test_verify_04(void) {
  meshlink_destroy("verifyconf");
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("verifyconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  char *data = "Test";
  char sig[MESHLINK_SIGLEN];
  size_t ssize = MESHLINK_SIGLEN;
  PRINT_TEST_CASE_MSG("Calling meshlink_sign to sign data\n");
  bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
  assert(ret);
  PRINT_TEST_CASE_MSG("meshlink_sign Successfuly signed data\n");

  PRINT_TEST_CASE_MSG("get nut node_handle\n");
  meshlink_node_t *source = meshlink_get_node(mesh_handle, "nut");
  assert(source != NULL);

  PRINT_TEST_CASE_MSG("Calling meshlink_verify API passing NULL as signed data argument\n");
  ret = meshlink_verify(mesh_handle, source, NULL, strlen(data) + 1, sig, ssize);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("verifyconf");

  if (!ret) {
    PRINT_TEST_CASE_MSG("meshlink_verify successfully reported NULL as data arg\n");
    return true;
  }
  PRINT_TEST_CASE_MSG("meshlink_verify FAILED to report NULL as data arg\n");
  return false;
}


/* Execute verify_data Test Case # 5 - Invalid case - meshlink_verify passing NULL args*/
void test_case_verify_05(void **state) {
    execute_test(test_verify_05, state);
    return;
}

/* Test Steps for meshlink_sign Test Case # 5 - Invalid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Sign data with meshlink_sign
    3. Trying to pass NULL as signature buffer argument
        and other arguments being valid

    Expected Result:
    Reports error accordingly by returning false
*/
bool test_verify_05(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  /* Create meshlink instance */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("verifyconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  char *data = "Test";
  char sig[MESHLINK_SIGLEN];
  size_t ssize = MESHLINK_SIGLEN;
  PRINT_TEST_CASE_MSG("Calling meshlink_sign to sign data\n");
  bool ret = meshlink_sign(mesh_handle, data, strlen(data) + 1, sig, &ssize);
  assert(ret);
  PRINT_TEST_CASE_MSG("meshlink_sign Successfuly signed data\n");

  PRINT_TEST_CASE_MSG("get nut node_handle\n");
  meshlink_node_t *source = meshlink_get_node(mesh_handle, "nut");
  assert(source != NULL);

  PRINT_TEST_CASE_MSG("Calling meshlink_verify API passing NULL as sign buffer argument\n");
  ret = meshlink_verify(mesh_handle, source, data, strlen(data) + 1, NULL, ssize);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("verifyconf");

  if(!ret) {
    PRINT_TEST_CASE_MSG("meshlink_verify successfully NULL as sign arg\n");
    return true;
  }
  PRINT_TEST_CASE_MSG("meshlink_verify FAILED to report NULL as sign arg\n");
  return false;
}

/* Execute verify_data Test Case # 6 - Functionality test, when a wrong source node is mentioned to verify
      the signed data */
void test_case_verify_06(void **state) {
    execute_test(test_verify_06, state);
    return;
}

/* Test Steps for meshlink_verify Test Case # 6 - Functionality Test

    Test Steps:
    1. Run NUT(Node Under Test) and peer
    2. Sign using peer as source node.
    3. Verify with NUT but passing NUT as source node rather than
        'peer' as source node

    Expected Result:
    API returns false when it detects the wrong source node
*/
bool test_verify_06(void) {
  /* deleting the confbase if already exists */
  meshlink_destroy("verifyconf1");
  meshlink_destroy("verifyconf2");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance for NUT */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("verifyconf1", "nut", "chat", DEV_CLASS_STATIONARY);
  if(!mesh1) {
    fprintf(stderr, "meshlink_open status for NUT: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1 != NULL);
  /* Create meshlink instance for bar */
  PRINT_TEST_CASE_MSG("Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("verifyconf2", "bar", "chat", DEV_CLASS_STATIONARY);
  if(!mesh2) {
    fprintf(stderr, "meshlink_open status for bar: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh2 != NULL);

  /* importing and exporting mesh meta data */
  char *exp1 = meshlink_export(mesh1);
  assert(exp1 != NULL);
  char *exp2 = meshlink_export(mesh2);
  assert(exp2 != NULL);
  assert(meshlink_import(mesh1, exp2));
  assert(meshlink_import(mesh2, exp1));
  PRINT_TEST_CASE_MSG("NUT and bar connected successfully\n");

  /* signing done by peer node  */
  char *data = "Test";
  char sig[MESHLINK_SIGLEN];
  size_t ssize = MESHLINK_SIGLEN;
  PRINT_TEST_CASE_MSG("Calling meshlink_sign to sign data\n");
  bool ret = meshlink_sign(mesh2, data, strlen(data) + 1, sig, &ssize);
  if (!ret) {
    PRINT_TEST_CASE_MSG("meshlink_verify FAILED to sign data\n");
    return false;
  }
  PRINT_TEST_CASE_MSG("meshlink_sign Successfuly signed data by 'peer' node\n");

  /* NUT tries to verify with source name NUT rather than peer */
  meshlink_node_t *source_nut = meshlink_get_self(mesh1);
  assert(source_nut != NULL);
  PRINT_TEST_CASE_MSG("NUT tries to verify the signed data with source name NUT rather than 'peer'\n");
  ret = meshlink_verify(mesh_handle, source_nut, data, strlen(data) + 1, sig, ssize);
  meshlink_stop(mesh1);
  meshlink_stop(mesh2);
  meshlink_close(mesh1);
  meshlink_close(mesh1);
  meshlink_destroy("verifyconf1");
  meshlink_destroy("verifyconf2");

  if(!ret) {
    PRINT_TEST_CASE_MSG("meshlink_verify successfully returned 'false' when a wrong source node used to verify the data\n");
    return true;
  }
  PRINT_TEST_CASE_MSG("meshlink_verify FAILED to report error when a wrong source is mentioned\n");
  return false;
}


int test_meshlink_verify(void) {
  const struct CMUnitTest blackbox_verify_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_verify_01, NULL, NULL,
            (void *)&test_case_verify_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_verify_02, NULL, NULL,
            (void *)&test_case_verify_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_verify_03, NULL, NULL,
            (void *)&test_case_verify_03_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_verify_04, NULL, NULL,
            (void *)&test_case_verify_04_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_verify_05, NULL, NULL,
            (void *)&test_case_verify_05_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_verify_06, NULL, NULL,
            (void *)&test_case_verify_06_state)
  };

  total_tests += sizeof(blackbox_verify_tests) / sizeof(blackbox_verify_tests[0]);

  return cmocka_run_group_tests(blackbox_verify_tests ,NULL , NULL);
 }
