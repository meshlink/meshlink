/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_get_fingerprint.c -- Execution of specific meshlink black box test cases
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

/*************************************************************************************
 *                          LOCAL MACROS                                             *
 *************************************************************************************/
/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/*************************************************************************************
 *                          LOCAL PROTOTYPES                                         *
 *************************************************************************************/
static void test_case_get_fingerprint_cb_01(void **state);
static bool test_get_fingerprint_cb_01(void);
static void test_case_get_fingerprint_cb_02(void **state);
static bool test_get_fingerprint_cb_02(void);
static void test_case_get_fingerprint_cb_03(void **state);
static bool test_get_fingerprint_cb_03(void);

/*************************************************************************************
 *                          LOCAL VARIABLES                                          *
 *************************************************************************************/
 /* State structure for get_fingerprint Test Case #1 */
static black_box_state_t test_case_get_fingerprint_cb_01_state = {
    /* test_case_name = */ "test_case_get_fingerprint_cb_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for get_fingerprint Test Case #2 */
static black_box_state_t test_case_get_fingerprint_cb_02_state = {
    /* test_case_name = */ "test_case_get_fingerprint_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for get_fingerprint Test Case #3 */
static black_box_state_t test_case_get_fingerprint_cb_03_state = {
    /* test_case_name = */ "test_case_get_fingerprint_cb_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* Execute get_fingerprint Test Case # 1 - Valid Case of obtaing publickey of NUT */
static void test_case_get_fingerprint_cb_01(void **state) {
    execute_test(test_get_fingerprint_cb_01, state);
    return;
}

/* Test Steps for get_fingerprint Test Case # 1 - Valid case

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Get node handle for ourself(for NUT) and obtain fingerprint

    Expected Result:
    Obtain fingerprint of NUT successfully.
*/
static bool test_get_fingerprint_cb_01(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ get_finger 01 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("getfingerprintconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  fprintf(stderr, "[ get_finger 01 ] Getting fingerprint of the NUT \n");
  char *fp = meshlink_get_fingerprint(mesh_handle, node);

  if (fp != NULL) {
    fprintf(stderr, "[ get_finger 01 ] Obtained fingerprint successfully \n");
    meshlink_close(mesh_handle);
    meshlink_destroy("getfingerprintconf");
    return true;
  }
  else {
    fprintf(stderr, "[ get_finger 01 ] failed to obtain fingerprint\n");
    meshlink_close(mesh_handle);
    meshlink_destroy("getfingerprintconf");
    return false;
  }
}

/* Execute get_fingerprint Test Case # 2 - Invalid Case - trying t0 obtain publickey of a node in a
   mesh by passing NULL as mesh handle argument*/
static void test_case_get_fingerprint_cb_02(void **state) {
    execute_test(test_get_fingerprint_cb_02, state);
    return;
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
  fprintf(stderr, "[ get_finger 02 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("getfingerprintconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  fprintf(stderr, "[ get_finger 02 ] passing NULL as mesh handle for meshlink_get_fingerprint API \n");
  char *fp = meshlink_get_fingerprint(NULL, node);

  if (fp == NULL) {
    fprintf(stderr, "[ get_finger 02 ] meshlink_get_fingerprint API reported error successfully \n");
    meshlink_close(mesh_handle);
    meshlink_destroy("getfingerprintconf");
    return true;
  }
  else {
    fprintf(stderr, "[ get_finger 02 ] failed to report error by meshlink_get_fingerprint API\n");
    meshlink_close(mesh_handle);
    meshlink_destroy("getfingerprintconf");
    return false;
  }
}

/* Execute get_fingerprint Test Case # 3 - Invalid Case - trying t0 obtain publickey of a node in a
   mesh by passing NULL as node handle argument */
static void test_case_get_fingerprint_cb_03(void **state) {
    execute_test(test_get_fingerprint_cb_03, state);
    return;
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
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  fprintf(stderr, "[ get_finger 03 ] Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("getfingerprintconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ get_finger 03 ] passing NULL as mesh handle for meshlink_get_fingerprint API \n");
  char *fp = meshlink_get_fingerprint(mesh_handle, NULL);

  if (fp == NULL) {
    fprintf(stderr, "[ get_finger 03 ] meshlink_get_fingerprint API reported error successfully \n");
    meshlink_close(mesh_handle);
    meshlink_destroy("getfingerprintconf");
    return true;
  }
  else {
    fprintf(stderr, "[ get_finger 03 ] failed to report error by meshlink_get_fingerprint API\n");
    meshlink_close(mesh_handle);
    meshlink_destroy("getfingerprintconf");
    return false;
  }
}


/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/

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

  return cmocka_run_group_tests(blackbox_get_fingerprint_tests ,NULL , NULL);

}
