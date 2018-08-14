/*
    test_cases_discovery.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_discovery.h"
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
/* Modify this to change the port number */
#define PORT 8000

static void test_case_discovery_01(void **state);
static bool test_steps_discovery_01(void);
static void test_case_discovery_02(void **state);
static bool test_steps_discovery_02(void);
static void test_case_discovery_03(void **state);
static bool test_steps_discovery_03(void);
static void status_callback(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach);

/* join_status gives us access to know whether node has joined or not */
static bool join_status;

/* mutex for the common variable */
static pthread_mutex_t lock;

/* State structure for discovery Test Case #1 */
static black_box_state_t test_case_discovery_01_state = {
    /* test_case_name = */ "test_case_discovery_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for discovery Test Case #2 */
static black_box_state_t test_case_discovery_02_state = {
    /* test_case_name = */ "test_case_discovery_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for discovery Test Case #3 */
static black_box_state_t test_case_discovery_03_state = {
    /* test_case_name = */ "test_case_discovery_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


static void status_callback(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
  fprintf(stderr, "In status callback\n");
  if (reach) {
    fprintf(stdout, "[ %s ] node reachable\n", source->name);
  }
  else {
    fprintf(stdout, "[ %s ] node not reachable\n", source->name) ;
  }
  pthread_mutex_lock(&lock);
  join_status = true;
  pthread_mutex_unlock(&lock);

  return;
}
/* Execute meshlink_discovery Test Case # 1 - connection with relay after being off-line*/
static void test_case_discovery_01(void **state) {
    execute_test(test_steps_discovery_01, state);
    return;
}

static bool test_steps_discovery_01(void) {
  bool ret ;

  meshlink_destroy("discconf1");
  meshlink_destroy("discconf2");
  pthread_mutex_lock(&lock);
  join_status = false;
  pthread_mutex_unlock(&lock);

  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance for NUT */
  fprintf(stderr, "[ discovery 01 ] Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("discconf1", "nut", "node_sim", DEV_CLASS_STATIONARY);
  if(!mesh1) {
    fprintf(stderr, "meshlink_open status for NUT: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1 != NULL);

  /* Create meshlink instance for bar */
  fprintf(stderr, "[ discovery 01 ] Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("discconf2", "bar", "node_sim", DEV_CLASS_STATIONARY);
  if(!mesh2) {
    fprintf(stderr, "meshlink_open status for bar: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh2 != NULL);

  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh1, status_callback);
  meshlink_set_node_status_cb(mesh2, NULL);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* importing and exporting mesh meta data */
  char *exp1 = meshlink_export(mesh1);
  assert(exp1 != NULL);
  char *exp2 = meshlink_export(mesh2);
  assert(exp2 != NULL);

  meshlink_enable_discovery(mesh1, true);
  meshlink_enable_discovery(mesh2, true);

  assert(meshlink_import(mesh1, exp2));
  assert(meshlink_import(mesh2, exp1));


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
  bool stat_ret = join_status;
  pthread_mutex_unlock(&lock);

  if (stat_ret) {
    PRINT_TEST_CASE_MSG("NUT discovered\n");
  }
  else {
    PRINT_TEST_CASE_MSG("NUT not being discovered\n");
  }

  meshlink_stop(mesh1);
  meshlink_stop(mesh2);
  meshlink_close(mesh1);
  meshlink_close(mesh2);
  meshlink_destroy("discconf1");
  meshlink_destroy("discconf2");

  return stat_ret;
}

/* Execute service discovery Test Case # 2 - Invalid case */
static void test_case_discovery_02(void **state) {
    execute_test(test_steps_discovery_02, state);
    return;
}

/* Test Steps for service discovery Test Case # 2 - Invalid case

    Test Steps:
    1. Pass meshlink_enable_discovery API with NULL as mesh handle argument.

    Expected Result:
    meshlink_enable_discovery returns proper error reporting.
*/
static bool test_steps_discovery_02(void) {
    fprintf(stderr, "[test_steps_discovery_02] Passing NULL: as meshlink_enable_discovery mesh argument\n");
     meshlink_enable_discovery(NULL, true);
     if (meshlink_errno == MESHLINK_EINVAL)
        return true;
    else
        return false;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
int test_meshlink_discovery(void) {
  const struct CMUnitTest blackbox_discovery_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_discovery_01, setup_test, teardown_test,
            (void *)&test_case_discovery_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_discovery_02, setup_test, teardown_test,
            (void *)&test_case_discovery_02_state)
  };
  total_tests += sizeof(blackbox_discovery_tests) / sizeof(blackbox_discovery_tests[0]);

  assert(pthread_mutex_init(&lock, NULL) == 0);
  int failed = cmocka_run_group_tests(blackbox_discovery_tests ,NULL , NULL);
  assert(pthread_mutex_destroy(&lock) == 0);

  return failed;
}
