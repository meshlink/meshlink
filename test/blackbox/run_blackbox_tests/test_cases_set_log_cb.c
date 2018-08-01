/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_set_log_cb.c -- Execution of specific meshlink black box test cases
 * @see
 * @author    Sai Roop, sairoop@elear.solutions
 * @copyright 2017  Guus Sliepen <guus@meshlink.io>
 *                  Manav Kumar Mehta <manavkumarm@yahoo.com>
 * @license   To any person (the "Recipient") obtaining a copy of this software and
 *            associated documentation files (the "Software"):\n
 *            All information contained in or disclosed by this software is
 *            confidential and proprietary information of Elear Solutions Tech
 *            Private Limited and all rights therein are expressly reserved.
 *            By accepting this material the recipient agrees that this material and
 *            the information contained therein is held in confidence and in trust
 *            and will NOT be used, copied, modified, merged, published, distributed,
 *            sublicensed, reproduced in whole or in part, nor its contents revealed
 *            in any manner to others without the express written permission of
 *            Elear Solutions Tech Private Limited.
 */
/*************************************************************************************/
/*===================================================================================*/
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

/*************************************************************************************
 *                          LOCAL MACROS                                             *
 *************************************************************************************/
/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/*************************************************************************************
 *                          LOCAL PROTOTYPES                                         *
 *************************************************************************************/
static void test_case_set_log_cb_01(void **state);
static bool test_set_log_cb_01(void);
static void test_case_set_log_cb_02(void **state);
static bool test_set_log_cb_02(void);
static void test_case_set_log_cb_03(void **state);
static bool test_set_log_cb_03(void);

/*************************************************************************************
 *                          LOCAL VARIABLES                                          *
 *************************************************************************************/
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

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
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
  fprintf(stderr, "[ log_cb 01 ] Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("logconf1", "nut", "chat", DEV_CLASS_STATIONARY);
  if(!mesh1) {
    fprintf(stderr, "meshlink_open status for NUT: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1 != NULL);

  /* Create meshlink instance for bar */
  fprintf(stderr, "[ log_cb 01 ] Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("logconf2", "bar", "chat", DEV_CLASS_STATIONARY);
  if(!mesh2) {
    fprintf(stderr, "meshlink_open status for bar: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh2 != NULL);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  fprintf(stderr, "[ log_cb 01] Setting valid log callback\n");
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, log_cb);

  sleep(1);

  /* importing and exporting mesh meta data */
  char *exp1 = meshlink_export(mesh1);
  assert(exp1 != NULL);
  char *exp2 = meshlink_export(mesh2);
  assert(exp2 != NULL);

  assert(meshlink_import(mesh1, exp2));
  assert(meshlink_import(mesh2, exp1));

  fprintf(stderr, "[log_cb 01] NUT and bar connected successfully\n");

  bool mesh1_start = meshlink_start(mesh1);
  if (!mesh1_start) {
    fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1_start);

  sleep(1);

  bool mesh2_start = meshlink_start(mesh2);
  if (!mesh2_start) {
  	fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh2_start);

  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = log;
  pthread_mutex_unlock(&lock);
  if (ret) {
    fprintf(stderr, "[ log_cb 01 ]Log call back invoked at least more than once\n");
  }
  else {
    fprintf(stderr, "[ log_cb 01 ]Log call back not invoked at least once\n");
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

  if (meshlink_errno != MESHLINK_EINVAL) {
    fprintf(stderr, "[ log_cb 01]No proper reporting for invalid level\n");
    return false;
  }
  else {
    fprintf(stderr, "[ log_cb 01]No proper reporting for invalid level\n");
    return true;
  }
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
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
