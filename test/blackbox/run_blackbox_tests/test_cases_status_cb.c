/*
    test_cases_status_cb.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_status_cb.h"
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

static void test_case_set_status_cb_01(void **state);
static bool test_set_status_cb_01(void);
static void test_case_set_status_cb_02(void **state);
static bool test_set_status_cb_02(void);
static void test_case_set_status_cb_03(void **state);
static bool test_set_status_cb_03(void);

 /* status variable gives access to the status callback to know whether invoked or not */
static bool status;

/* mutex for the common variable */
pthread_mutex_t lock;

/* State structure for status callback Test Case #1 */
static char *test_stat_1_nodes[] = { "relay", "peer" };
static black_box_state_t test_case_set_status_cb_01_state = {
    /* test_case_name = */ "test_case_set_status_cb_01",
    /* node_names = */ test_stat_1_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for status callback Test Case #2 */
static black_box_state_t test_case_set_status_cb_02_state = {
    /* test_case_name = */ "test_case_set_status_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


static void status_cb(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
  fprintf(stderr, "In status callback\n");
  if (reach) {
    fprintf(stdout, "[ %s ] node reachable\n", source->name);
  }
  else {
    fprintf(stdout, "[ %s ] node not reachable\n", source->name) ;
  }

  pthread_mutex_lock(&lock);
  status = true;
  pthread_mutex_unlock(&lock);
  return;
}

  int black_box_group_status_setup(void **state) {
    char *nodes[] = { "relay" };
    int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

    printf("Creating Containers\n");
    destroy_containers();
    create_containers(nodes, num_nodes);

    return 0;
}

 int black_box_group_status_teardown(void **state) {
    printf("Destroying Containers\n");
    destroy_containers();

    return 0;
}


/* Execute status callback Test Case # 1 - valid case */
static void test_case_set_status_cb_01(void **state) {
    execute_test(test_set_status_cb_01, state);
    return;
}

/* Test Steps for meshlink_set_status_cb Test Case # 1

    Test Steps:
    1. Run relay and Open NUT
    2. Set log callback for the NUT and Start NUT

    Expected Result:
    status callback should be invoked when NUT connects/disconnects with 'relay' node.
*/
static bool test_set_status_cb_01(void) {
  meshlink_destroy("set_status_cb_conf");

  PRINT_TEST_CASE_MSG("Running relay node\n");
  /* generate invite in relay container */
  char *invite_nut = invite_in_container("relay", "nut");
  node_sim_in_container("relay", "1", NULL);

  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  mesh_handle = meshlink_open("joinconf", "nut", "node_sim", 1);
  PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Set up callback for node status (reachable / unreachable) */
  pthread_mutex_lock(&lock);
  status = false;
  pthread_mutex_unlock(&lock);
  meshlink_set_node_status_cb(mesh_handle, status_cb);
  sleep(2);

  /*Joining the NUT with relay*/
  assert(meshlink_join(mesh_handle, invite_nut));
  PRINT_TEST_CASE_MSG("meshlink_join status: %s\n", meshlink_strerror(meshlink_errno));
  assert(meshlink_start(mesh_handle));
  sleep(2);

  pthread_mutex_lock(&lock);
  bool ret = status;
  pthread_mutex_unlock(&lock);
  if(ret) {
    PRINT_TEST_CASE_MSG("Status callback invoked when 'relay' is reachable or unreachable\n");
  }
  else {
    PRINT_TEST_CASE_MSG("Status callback not invoked when 'relay' is reachable or unreachable\n");
  }

  free(invite_nut);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("set_status_cb_conf");

  return ret;
}

/* Execute status callback Test Case # 2 - Invalid case */
static void test_case_set_status_cb_02(void **state) {
    execute_test(test_set_status_cb_02, state);
    return;
}

/* Test Steps for meshlink_set_status_cb Test Case # 2

    Test Steps:
    1. Calling meshlink_set_status_cb with NULL as mesh handle argument.

    Expected Result:
    set poll callback handles the invalid parameter when called by giving proper error number.
*/
static bool test_set_status_cb_02(void) {
  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("setstat02conf", "nut", "node_sim", 1);
  PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);

  meshlink_errno_t meshlink_errno_buff = meshlink_errno;
  PRINT_TEST_CASE_MSG("Setting callback API with mesh handle as NULL\n");
  meshlink_set_node_status_cb(NULL, status_cb);
  if(MESHLINK_EINVAL == meshlink_errno ) {
    PRINT_TEST_CASE_MSG("Setting callback API with mesh handle as NULL reported SUCCESSFULY\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("API with mesh handle as NULL failured to report error\n");
    return false;
  }
}


int test_meshlink_set_status_cb(void) {
  const struct CMUnitTest blackbox_status_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_set_status_cb_01, setup_test, teardown_test,
            (void *)&test_case_set_status_cb_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_set_status_cb_02, NULL, NULL,
            (void *)&test_case_set_status_cb_02_state)
  };
  total_tests += sizeof(blackbox_status_tests) / sizeof(blackbox_status_tests[0]);

  assert(pthread_mutex_init(&lock, NULL) == 0);

  int failed = cmocka_run_group_tests(blackbox_status_tests, black_box_group_status_setup, black_box_group_status_teardown);

  pthread_mutex_destroy(&lock);

  return failed;
}
