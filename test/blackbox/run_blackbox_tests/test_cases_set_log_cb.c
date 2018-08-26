/*
    test_cases_set_log_cb.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_set_log_cb.h"
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

static void test_case_set_log_cb_01(void **state);
static bool test_set_log_cb_01(void);
static void test_case_set_log_cb_02(void **state);
static bool test_set_log_cb_02(void);
static void test_case_set_log_cb_03(void **state);
static bool test_set_log_cb_03(void);

/* log variable gives access to the log callback to know whether invoked or not */
static bool log;

/* mutex for the common variable */
pthread_mutex_t lock;

/* State structure for log callback Test Case #1 */
static char *test_log_1_nodes[] = { "relay" };
static black_box_state_t test_case_set_log_cb_01_state = {
    /* test_case_name = */ "test_case_set_log_cb_01",
    /* node_names = */ test_log_1_nodes,
    /* num_nodes = */ 1,
    /* test_result (defaulted to) = */ false
};

/* State structure for log callback Test Case #2 */
static black_box_state_t test_case_set_log_cb_02_state = {
    /* test_case_name = */ "test_case_set_log_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/* log callback */
static void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
  fprintf(stderr, "In log callback\n");
  fprintf(stderr, "Received log text : %s\n", text);

  pthread_mutex_lock(&lock);
  log = true;
  pthread_mutex_unlock(&lock);

  return;
}

/* Execute meshlink_set_log_cb Test Case # 1 - Valid case */
static void test_case_set_log_cb_01(void **state) {
    execute_test(test_set_log_cb_01, state);
    return;
}
/* Test Steps for meshlink_set_receive_cb Test Case # 1

    Test Steps:
    1. Run relay and Open NUT
    2. Set log callback for the NUT and Start NUT

    Expected Result:
    log callback should be invoked when NUT joins with relay.
*/
static bool test_set_log_cb_01(void) {
  meshlink_destroy("logconf1");
  meshlink_destroy("logconf2");

  /* Set up logging for Meshlink */
  pthread_mutex_lock(&lock);
  log = false;
  pthread_mutex_unlock(&lock);
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, log_cb);

  /* Create meshlink instance for NUT */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("logconf1", "nut", "chat", DEV_CLASS_STATIONARY);
  if(!mesh1) {
    fprintf(stderr, "meshlink_open status for NUT: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1 != NULL);

  /* Create meshlink instance for bar */
  PRINT_TEST_CASE_MSG("Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("logconf2", "bar", "chat", DEV_CLASS_STATIONARY);
  if(!mesh2) {
    fprintf(stderr, "meshlink_open status for bar: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh2 != NULL);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  PRINT_TEST_CASE_MSG("Setting valid log callback\n");
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, log_cb);

  sleep(1);

  /* importing and exporting mesh meta data */
  char *exp1 = meshlink_export(mesh1);
  assert(exp1 != NULL);
  char *exp2 = meshlink_export(mesh2);
  assert(exp2 != NULL);
  assert(meshlink_import(mesh1, exp2));
  assert(meshlink_import(mesh2, exp1));
  PRINT_TEST_CASE_MSG("NUT and bar connected successfully\n");

  bool mesh1_start = meshlink_start(mesh1);
  if(!mesh1_start) {
    fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1_start);
  sleep(1);
  bool mesh2_start = meshlink_start(mesh2);
  if(!mesh2_start) {
  	fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh2_start);
  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = log;
  pthread_mutex_unlock(&lock);
  if(ret) {
    PRINT_TEST_CASE_MSG("Log call back invoked at least more than once\n");
  } else {
    PRINT_TEST_CASE_MSG("Log call back not invoked at least once\n");
  }
  /* closing meshes and destroying confbase */
  meshlink_stop(mesh1);
  meshlink_stop(mesh2);
  meshlink_close(mesh1);
  meshlink_close(mesh2);
  meshlink_destroy("logconf1");
  meshlink_destroy("logconf2");

  return ret;
}

/* Execute meshlink_set_log_cb Test Case # 2 - Invalid case */
static void test_case_set_log_cb_02(void **state) {
    execute_test(test_set_log_cb_02, state);
    return;
}
/* Test Steps for meshlink_set_poll_cb Test Case # 2

    Test Steps:
    1. Calling meshlink_set_poll_cb with some invalid integer other than the valid enums.

    Expected Result:
    set poll callback handles the invalid parameter when called by giving proper error number.
*/
static bool test_set_log_cb_02(void) {
  /*Setting an invalid level*/
  meshlink_set_log_cb(NULL, 1000, NULL);

  if(meshlink_errno != MESHLINK_EINVAL) {
    PRINT_TEST_CASE_MSG("No proper reporting for invalid level\n");
    return false;
  } else {
    PRINT_TEST_CASE_MSG("No proper reporting for invalid level\n");
    return true;
  }
}


int test_meshlink_set_log_cb(void) {
  const struct CMUnitTest blackbox_log_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_set_log_cb_01, NULL, NULL,
            (void *)&test_case_set_log_cb_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_set_log_cb_02, NULL, NULL,
            (void *)&test_case_set_log_cb_02_state)
  };
  total_tests += sizeof(blackbox_log_tests) / sizeof(blackbox_log_tests[0]);

  assert(pthread_mutex_init(&lock, NULL) == 0);

  int failed = cmocka_run_group_tests(blackbox_log_tests, NULL, NULL);

  pthread_mutex_destroy(&lock);

  return failed;
}
