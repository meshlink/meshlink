/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_set_port.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_destroy.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_set_port.h"
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
static void test_case_set_port_01(void **state);
static bool test_set_port_01(void);
static void test_case_set_port_02(void **state);
static bool test_set_port_02(void);
static void test_case_set_port_03(void **state);
static bool test_set_port_03(void);

/*************************************************************************************
 *                          LOCAL VARIABLES                                          *
 *************************************************************************************/
 /* State structure for set port API Test Case #1 */
static black_box_state_t test_case_set_port_01_state = {
    /* test_case_name = */ "test_case_set_port_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for set port API Test Case #2 */
static black_box_state_t test_case_set_port_02_state = {
    /* test_case_name = */ "test_case_set_port_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for set port API Test Case #3 */
static black_box_state_t test_case_set_port_03_state = {
    /* test_case_name = */ "test_case_set_port_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/

/* Execute meshlink_set_port Test Case # 1 - valid case*/
static void test_case_set_port_01(void **state) {
    execute_test(test_set_port_01, state);
    return;
}

/* Test Steps for meshlink_set_port Test Case # 1 - Valid case

    Test Steps:
    1. Open NUT(Node Under Test)
    2. Set Port for NUT

    Expected Result:
    Set the new port to the NUT.
*/
static bool test_set_port_01(void) {
  fprintf(stderr, "[ set_port 01 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  mesh_handle = meshlink_open("setportconf", "nut", "node_sim", 1);
  PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  int port;

  port = meshlink_get_port(mesh_handle);
  assert(port > 0);
  fprintf(stderr, "[ set_port 01 ] port of NUT before setting new port : %d \n", port);

  fprintf(stderr, "[ set_port 01 ] seting port 8000 using meshlink_set_port API \n");
  bool ret = meshlink_set_port(mesh_handle, 8000);

  port = meshlink_get_port(mesh_handle);
  assert(port > 0);
  fprintf(stderr, "[ set_port 01 ] port of NUT after setting new port(8000) : %d \n", port);

  if (ret && (port == 8000)) {
    fprintf(stderr, "[ set_port 01 ] Port set successfully\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("setportconf");
    meshlink_destroy("setportconf");
    meshlink_destroy("setportconf");
    return true;
  }
  else {
    fprintf(stderr, "[ set_port 01 ] failed to set port\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("exportconf");
    return false;
  }

}

/* Execute meshlink_set_port Test Case # 2 - Invalid case*/
static void test_case_set_port_02(void **state) {
    execute_test(test_set_port_02, state);
    return;
}

/* Test Steps for meshlink_set_port Test Case # 2 - Invalid case

    Test Steps:
    1. Pass NULL as mesh handle argument for meshlink_set_port

    Expected Result:
    Report false indicating error.
*/
static bool test_set_port_02(void) {
  fprintf(stderr, "[ set_port 02 ]Setting NULL as mesh handle\n");
  bool ret = meshlink_set_port(NULL, 8000);

  if ( (false == ret) && (MESHLINK_EINVAL == meshlink_errno))  {
    fprintf(stderr, "[ set_port 02 ]NULL argument reported SUCCESSFULY\n");
    return true;
  }

  fprintf(stderr, "[ set_port 02 ]failed to report NULL argument\n");
  return false;
}


/* Execute meshlink_set_port Test Case # 3 - Setting port after starting mesh*/
static void test_case_set_port_03(void **state) {
  execute_test(test_set_port_03, state);
  return;
}

/* Test Steps for meshlink_set_port Test Case # 2 - functionality test

    Test Steps:
    1. Open and start NUT and then try to set new port number

    Expected Result:
    New port number cannot be set while mesh is running.
*/
static bool test_set_port_03(void) {
  fprintf(stderr, "[ set_port 03 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  mesh_handle = meshlink_open("getportconf", "nut", "node_sim", 1);
  PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ set_port 03 ] Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  fprintf(stderr, "[ set_port 03 ] Setting port after starting NUT\n");
  bool ret = meshlink_set_port(mesh_handle, 50000);
  if (!ret) {
    fprintf(stderr, "meshlink_set_port status : %s\n", meshlink_strerror(meshlink_errno));
  }

  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("getportconf");

  if (!ret) {
    fprintf(stderr, "[ set_port 03 ] New port cannot be set after starting mesh \n");
    return true;
  }
  else {
    fprintf(stderr, "[ set_port 03 ] Port can be set even after starting mesh \n");
    return false;
  }
}


/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
 int test_meshlink_set_port(void) {
  const struct CMUnitTest blackbox_set_port_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_set_port_01, NULL, NULL,
          (void *)&test_case_set_port_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_set_port_02, NULL, NULL,
          (void *)&test_case_set_port_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_set_port_03, NULL, NULL,
          (void *)&test_case_set_port_03_state)
    };
    total_tests += sizeof(blackbox_set_port_tests) / sizeof(blackbox_set_port_tests[0]);
 return cmocka_run_group_tests(blackbox_set_port_tests, NULL, NULL);
}
