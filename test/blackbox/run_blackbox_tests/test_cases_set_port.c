/*
    test_cases_set_port.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_set_port.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>


/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_set_port_01(void **state);
static bool test_set_port_01(void);
static void test_case_set_port_02(void **state);
static bool test_set_port_02(void);
static void test_case_set_port_03(void **state);
static bool test_set_port_03(void);

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
  PRINT_TEST_CASE_MSG("port of NUT before setting new port : %d \n", port);
  PRINT_TEST_CASE_MSG("seting port 8000 using meshlink_set_port API \n");
  bool ret = meshlink_set_port(mesh_handle, 8000);

  port = meshlink_get_port(mesh_handle);
  assert(port > 0);
  PRINT_TEST_CASE_MSG("port of NUT after setting new port(8000) : %d \n", port);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("setportconf");

  if(ret && (port == 8000)) {
    PRINT_TEST_CASE_MSG("Port set successfully\n");
    return true;
  } else {
    fprintf(stderr, "[ set_port 01 ] failed to set port\n");
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
  PRINT_TEST_CASE_MSG("Setting NULL as mesh handle\n");
  bool ret = meshlink_set_port(NULL, 8000);

  if((false == ret) && (MESHLINK_EINVAL == meshlink_errno))  {
    PRINT_TEST_CASE_MSG("NULL argument reported SUCCESSFULY\n");
    return true;
  }
  PRINT_TEST_CASE_MSG("failed to report NULL argument\n");
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
  PRINT_TEST_CASE_MSG(" Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  mesh_handle = meshlink_open("getportconf", "nut", "node_sim", 1);
  PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  PRINT_TEST_CASE_MSG(" Starting NUT\n");
  assert(meshlink_start(mesh_handle));

  PRINT_TEST_CASE_MSG("Setting port after starting NUT\n");
  bool ret = meshlink_set_port(mesh_handle, 50000);
  if(!ret) {
    fprintf(stderr, "meshlink_set_port status : %s\n", meshlink_strerror(meshlink_errno));
  }

  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("getportconf");

  if(!ret) {
    PRINT_TEST_CASE_MSG(" New port cannot be set after starting mesh \n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("Port can be set even after starting mesh \n");
    return false;
  }
}


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
