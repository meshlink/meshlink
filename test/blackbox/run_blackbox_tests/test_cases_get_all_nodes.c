/*
    test_cases_get_all_nodes.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_get_all_nodes.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>


/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_get_all_nodes_01(void **state);
static bool test_get_all_nodes_01(void);
static void test_case_get_all_nodes_02(void **state);
static bool test_get_all_nodes_02(void);
static void test_case_get_all_nodes_03(void **state);
static bool test_get_all_nodes_03(void);
static void test_case_get_all_nodes_04(void **state);
static bool test_get_all_nodes_04(void);

/* State structure for get_all_nodes Test Case #1 */
static black_box_state_t test_case_get_all_nodes_01_state = {
    /* test_case_name = */ "test_case_get_all_nodes_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
/* State structure for get_all_nodes Test Case #2 */
static black_box_state_t test_case_get_all_nodes_02_state = {
    /* test_case_name = */ "test_case_get_all_nodes_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
/* State structure for get_all_nodes Test Case #3 */
static black_box_state_t test_case_get_all_nodes_03_state = {
    /* test_case_name = */ "test_case_get_all_nodes_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
/* State structure for get_all_nodes Test Case #4 */
static black_box_state_t test_case_get_all_nodes_04_state = {
    /* test_case_name = */ "test_case_get_all_nodes_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* Execute get_all_nodes Test Case # 1 - Valid case - get all nodes in the mesh */
static void test_case_get_all_nodes_01(void **state) {
  execute_test(test_get_all_nodes_01, state);
  return;
}
/* Test Steps for get_all_nodes Test Case # 1 - Valid case

    Test Steps:
    1. Open NUT and get list of nodes
    2. Open bar and join with NUT
    3. get list of nodes together

    Expected Result:
    Obtaining list of nodes in the mesh at the given instance
*/
static bool test_get_all_nodes_01(void) {
  /* deleting the confbase if already exists */
  meshlink_destroy("getnodeconf1");
  meshlink_destroy("getnodeconf2");

  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  /* Create meshlink instance for NUT */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("getnodeconf1", "nut", "node_sim", DEV_CLASS_STATIONARY);
  if(!mesh1) {
    fprintf(stderr, "meshlink_open status for NUT: %s\n", meshlink_strerror(meshlink_errno));
  }
  assert(mesh1 != NULL);

  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  size_t nnodes = 0;
  meshlink_node_t **nodes = NULL;

  PRINT_TEST_CASE_MSG("Calling meshlink_get_all_nodes with NULL for new allocation\n");
  nodes = meshlink_get_all_nodes(mesh1, nodes, &nnodes);
  if((!nodes) && (nnodes != 1)) {
    PRINT_TEST_CASE_MSG("Failed to get nodes\n");
    meshlink_stop(mesh1);
    meshlink_close(mesh1);
    meshlink_destroy("getnodesconf1");
    return false;
  } else {
    PRINT_TEST_CASE_MSG("meshlink_get_all_nodes returned list of nodes successfully\n");
  }

  /* Create meshlink instance for bar */
  PRINT_TEST_CASE_MSG("Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("getnodeconf2", "bar", "node_sim", DEV_CLASS_STATIONARY);
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

  PRINT_TEST_CASE_MSG("Calling meshlink_get_all_nodes with same node array for re-allocation\n");
  nodes = meshlink_get_all_nodes(mesh1, nodes, &nnodes);
  meshlink_stop(mesh1);
  meshlink_stop(mesh2);
  meshlink_close(mesh1);
  meshlink_close(mesh2);
  meshlink_destroy("getnodeconf1");
  meshlink_destroy("getnodeconf2");
  if((!nodes) && (nnodes != 2)) {
    PRINT_TEST_CASE_MSG("Failed to get nodes\n");
    return false;
  } else {
    PRINT_TEST_CASE_MSG("meshlink_get_all_nodes reallocated and returned updated list of nodes\n");
    return true;
  }
}



/* Execute get_all_nodes Test Case # 2 - Invalid case - get all nodes in the mesh passing NULL */
static void test_case_get_all_nodes_02(void **state) {
    execute_test(test_get_all_nodes_02, state);
    return;
}

/* Test Steps for get_all_nodes Test Case # 2 - Invalid case

    Test Steps:
    1. Passing NULL as mesh handle argument for meshlink_get_all_nodes

    Expected Result:
    Error reported correctly by returning NULL
*/
static bool test_get_all_nodes_02(void) {
  meshlink_node_t **nodes = NULL;
  size_t nmemb = 0;

  PRINT_TEST_CASE_MSG("Passing NULL as argument for mesh handle to meshlink_get_all_nodes API \n");
  meshlink_node_t **node = meshlink_get_all_nodes(NULL, nodes, &nmemb);
  fprintf(stderr, "meshlink_get_all_nodes: %s\n", meshlink_strerror(meshlink_errno));
  if(!node) {
    PRINT_TEST_CASE_MSG("get all nodes API successfuly returned failure on passing NULL as mesh handle arg\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("get all nodes API didnt report failure on passing NULL as mesh handle arg\n");
    return false;
  }
}

/* Execute get_all_nodes Test Case # 3 - Invalid case - get all nodes in the mesh passing NULL as nodes arg */
static void test_case_get_all_nodes_03(void **state) {
    execute_test(test_get_all_nodes_03, state);
    return;
}
/* Test Steps for get_all_nodes Test Case # 3 - Valid case

    Test Steps:
    1. Passing NULL as pointer to array of node handle argument for meshlink_get_all_nodes

    Expected Result:
    Error reported correctly by returning NULL
*/
static bool test_get_all_nodes_03(void) {
  /* Create meshlink instance */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("getallnodesconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  assert(meshlink_start(mesh_handle));

  size_t nmemb = 0;
  PRINT_TEST_CASE_MSG("Passing NULL as nodes array argument \n");
  meshlink_node_t **nodes = meshlink_get_all_nodes(mesh_handle, NULL, &nmemb);
  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("getallnodesconf");

  if((nodes != NULL) && (nmemb == 1)) {
    PRINT_TEST_CASE_MSG("get_all_nodes successfully returned a pointer to array of nodes when NULL being passed as argument\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("get_all_nodes Reported failure on passing NULL for pointer to array of node handle arg\n");
    return false;
  }
}



/* Execute get_all_nodes Test Case # 4 - Invalid case - get all nodes in the mesh passing NULL as nmeb arg */
static void test_case_get_all_nodes_04(void **state) {
    execute_test(test_get_all_nodes_04, state);
    return;
}
/* Test Steps for get_all_nodes Test Case # 4 - Invalid case

    Test Steps:
    1. Passing NULL as pointer to node members argument for meshlink_get_all_nodes

    Expected Result:
    Error reported correctly by returning NULL
*/
static bool test_get_all_nodes_04(void) {
  /* Create meshlink instance */
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  meshlink_handle_t *mesh_handle = meshlink_open("getallnodesconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  assert(meshlink_start(mesh_handle));

  meshlink_node_t **nodes = NULL;
  PRINT_TEST_CASE_MSG("Passing NULL as nmembers argument \n");
  nodes = meshlink_get_all_nodes(mesh_handle, nodes, NULL);

  if(!nodes) {
    PRINT_TEST_CASE_MSG("get_all_nodes Reported failure succesfully\n");
    meshlink_destroy("getallnodesconf");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return true;
  } else {
    PRINT_TEST_CASE_MSG("get_all_nodes Reported failure succesfully\n");
    meshlink_destroy("getallnodesconf");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return false;
  }
}


int test_meshlink_get_all_nodes(void) {
  const struct CMUnitTest blackbox_get_all_nodes[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_get_all_nodes_01, NULL, NULL,
          (void *)&test_case_get_all_nodes_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_get_all_nodes_02, NULL, NULL,
          (void *)&test_case_get_all_nodes_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_get_all_nodes_03, NULL, NULL,
          (void *)&test_case_get_all_nodes_03_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_get_all_nodes_04, NULL, NULL,
          (void *)&test_case_get_all_nodes_04_state)
  };
  total_tests += sizeof(blackbox_get_all_nodes) / sizeof(blackbox_get_all_nodes[0]);

  return cmocka_run_group_tests(blackbox_get_all_nodes, NULL, NULL);
}
