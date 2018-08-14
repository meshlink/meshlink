/*
    test_cases_rec_cb.c -- Execution of specific meshlink black box test cases
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
#include "test_cases.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_rec_cb.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <pthread.h>


/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_set_rec_cb_01(void **state);
static bool test_set_rec_cb_01(void);
static void test_case_set_rec_cb_02(void **state);
static bool test_set_rec_cb_02(void);
static void test_case_set_rec_cb_03(void **state);
static bool test_set_rec_cb_03(void);
static void test_case_set_rec_cb_04(void **state);
static bool test_set_rec_cb_04(void);

 /* State structure for Meta-connections Test Case #1 */
static black_box_state_t test_case_set_rec_cb_01_state = {
    /* test_case_name = */ "test_case_set_rec_cb_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
/* State structure for Meta-connections Test Case #1 */
static black_box_state_t test_case_set_rec_cb_02_state = {
    /* test_case_name = */ "test_case_set_rec_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
/* State structure for Meta-connections Test Case #1 */
static black_box_state_t test_case_set_rec_cb_03_state = {
    /* test_case_name = */ "test_case_set_rec_cb_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
/* call variable gives access to deduce whether receive callback has invoked or not */
static bool call;
/* mutex for the common variable */
pthread_mutex_t lock;

/* receive callback function */
static void rec_cb(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
   fprintf(stderr, "In receive callback\n");
   fprintf(stderr, "Received message : %s\n", (char *)data);

   pthread_mutex_lock(&lock);
   call = true;
   pthread_mutex_unlock(&lock);
   return;
}

/* Execute meshlink_set_receive_cb Test Case # 1 - Valid case */
static void test_case_set_rec_cb_01(void **state) {
    execute_test(test_set_rec_cb_01, state);
    return;
}
/* Test Steps for meshlink_set_receive_cb Test Case # 1

    Test Steps:
    1. Open NUT
    2. Set receive callback for the NUT
    3. Echo NUT with some data.

    Expected Result:
    Receive callback should be invoked when NUT echoes or sends data for itself.
*/
static bool test_set_rec_cb_01(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("set_receive_cb_conf", "nut", "node_sim", 1);
  assert(mesh_handle);
  /* Setting  receive callback */
  PRINT_TEST_CASE_MSG("Setting Valid receive callback\n");
  meshlink_set_receive_cb(mesh_handle, rec_cb);
  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Starting mesh\n");
  assert(meshlink_start(mesh_handle));
  sleep(1);

  PRINT_TEST_CASE_MSG("Sending Message\n");
  char *data = "Test message";
  /* making 'call' variable default false if receive callback not invoked */
  pthread_mutex_lock(&lock);
  call = false;
  pthread_mutex_unlock(&lock);

  meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);
  assert(node_handle);
  assert(meshlink_send(mesh_handle, node_handle, data, strlen(data) + 1));
  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = call;
  pthread_mutex_unlock(&lock);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("set_receive_cb_conf");

  if(ret) {
    PRINT_TEST_CASE_MSG("Invoked callback\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("No callback invoked\n");
    return false;
  }
}


/* Execute meshlink_set_receive_cb Test Case # 2 - Invalid case */
static void test_case_set_rec_cb_02(void **state) {
    execute_test(test_set_rec_cb_02, state);
    return;
}
/* Test Steps for meshlink_set_receive_cb Test Case # 2

    Test Steps:
    1. Call meshlink_set_receive_cb with NULL as mesh handle argument

    Expected Result:
    meshlink_set_receive_cb API reports proper error accordingly.
*/
static bool test_set_rec_cb_02(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  PRINT_TEST_CASE_MSG("Setting receive callback with NULL as mesh handle\n");
  meshlink_set_receive_cb(NULL, rec_cb);
  if(meshlink_errno == MESHLINK_EINVAL ) {
    PRINT_TEST_CASE_MSG("meshlink_set_receive_cb API reports Invalid argument error successfully\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("meshlink_set_receive_cb API failed to report Invalid argument error\n");
    return false;
  }
}

/* Execute meshlink_set_receive_cb Test Case # 3 - Functionality Test, Trying to set receive call back after
      starting the mesh */
static void test_case_set_rec_cb_03(void **state) {
    execute_test(test_set_rec_cb_03, state);
    return;
}
/* Test Steps for meshlink_set_receive_cb Test Case # 1

    Test Steps:
    1. Open NUT
    2. Starting mesh
    2. Set receive callback for the NUT
    3. Echo NUT with some data.

    Expected Result:
    Receive callback can be invoked when NUT echoes or sends data for itself
*/
static bool test_set_rec_cb_03(void) {
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  /* Create meshlink instance */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("set_receive_cb_conf", "nut", "node_sim", 1);
  assert(mesh_handle);
  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  PRINT_TEST_CASE_MSG("Starting mesh\n");
  assert(meshlink_start(mesh_handle));
  /* Setting  receive callback */
  PRINT_TEST_CASE_MSG("Setting Valid receive callback\n");
  meshlink_set_receive_cb(mesh_handle, rec_cb);
  sleep(1);

  PRINT_TEST_CASE_MSG("Sending Message\n");
  char *data = "Test message";
  /* making 'call' variable default false if receive callback not invoked */
  pthread_mutex_lock(&lock);
  call = false;
  pthread_mutex_unlock(&lock);

  meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);
  assert(node_handle);
  assert(meshlink_send(mesh_handle, node_handle, data, strlen(data) + 1));
  sleep(1);

  pthread_mutex_lock(&lock);
  bool ret = call;
  pthread_mutex_unlock(&lock);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("set_receive_cb_conf");
  if(ret) {
    PRINT_TEST_CASE_MSG("Invoked callback when receive callback has been set even after starting mesh\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("No callback invoked when receive callback has been set after starting mesh\n");
    return false;
  }
}

int test_meshlink_set_receive_cb(void) {
  const struct CMUnitTest blackbox_receive_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_01, NULL, NULL,
            (void *)&test_case_set_rec_cb_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_02, NULL, NULL,
            (void *)&test_case_set_rec_cb_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_03, NULL, NULL,
            (void *)&test_case_set_rec_cb_03_state)
  };
  total_tests += sizeof(blackbox_receive_tests) / sizeof(blackbox_receive_tests[0]);

  assert(pthread_mutex_init(&lock, NULL) == 0);
  int failed = cmocka_run_group_tests(blackbox_receive_tests ,NULL , NULL);
  assert(pthread_mutex_destroy(&lock) == 0);

  return failed;
}
