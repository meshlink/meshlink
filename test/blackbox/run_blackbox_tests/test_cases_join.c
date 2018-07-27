/*
    test_cases.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_join.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static bool join_status;
static void status_callback(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
   fprintf(stderr, "In status callback\n");
   if (reach) {
     fprintf(stdout, "[ %s ] node reachable\n", source->name);
   }
   else {
     fprintf(stdout, "[ %s ] node not reachable\n", source->name) ;
   }

   if (0 == strcmp(source->name, "relay")) {
     join_status = true;
    PRINT_TEST_CASE_MSG("NUT joined with relay\n");
   }
   else {
     join_status = false;
    PRINT_TEST_CASE_MSG("NUT didnt join with relay but with some other node\n");
   }
   return;
}

/* Execute join Test Case # 1 - valid case*/
void test_case_meshlink_join_01(void **state) {
    execute_test(test_meshlink_join_01, state);
    return;
}

bool test_meshlink_join_01(void) {
  join_status = false;
  bool ret ;

  meshlink_destroy("joinconf");
  char *invite_nut = invite_in_container("relay", NUT_NODE_NAME);
  node_sim_in_container("relay", "1", NULL);

    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("joinconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, status_callback);

    sleep(2);
    /*Joining the NUT with relay*/
   ret = meshlink_join(mesh_handle, invite_nut);
   assert(ret);
    PRINT_TEST_CASE_MSG("meshlink_join status: %s\n", meshlink_strerror(meshlink_errno));
    assert(meshlink_start(mesh_handle));

    sleep(2);

    if (join_status) {
    PRINT_TEST_CASE_MSG("after 2 seconds NUT joined with relay\n");
    }
    else {
    PRINT_TEST_CASE_MSG("after 2 seconds NUT didn't join with relay\n");
    }
    free(invite_nut);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
  meshlink_destroy("joinconf");
    return join_status && ret;
}

/* Execute join Test Case # 1 - valid case*/
void test_case_meshlink_join_02(void **state) {
    execute_test(test_meshlink_join_02, state);
    return;
}

bool test_meshlink_join_02(void) {
  bool ret ;

  char *invite_nut = invite_in_container("relay", NUT_NODE_NAME);

  /*Joining the NUT with relay*/
  ret = meshlink_join(NULL, invite_nut);
  PRINT_TEST_CASE_MSG("meshlink_join status: %s\n", meshlink_strerror(meshlink_errno));
  free(invite_nut);

  if (ret) {
  PRINT_TEST_CASE_MSG("meshlink_join reported error accordingly\n");
  }
  else {
  PRINT_TEST_CASE_MSG("meshlink_join failed to report error accordingly\n");
  }

  return !ret;
}

/* Execute join Test Case # 1 - valid case*/
void test_case_meshlink_join_03(void **state) {
    execute_test(test_meshlink_join_03, state);
    return;
}

bool test_meshlink_join_03(void) {
  bool ret = false;

  meshlink_destroy("joinconf");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  mesh_handle = meshlink_open("joinconf", "nut", "node_sim", 1);
  PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);
  /* Set up logging for Meshlink with the newly acquired Mesh Handle */
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  /* Set up callback for node status (reachable / unreachable) */
  meshlink_set_node_status_cb(mesh_handle, status_callback);

  /*Joining the NUT with relay*/
  meshlink_join(mesh_handle, NULL);
  PRINT_TEST_CASE_MSG("meshlink_join status: %s\n", meshlink_strerror(meshlink_errno));

  if (ret) {
  PRINT_TEST_CASE_MSG("meshlink_join reported error accordingly when NULL is passed as invite argument\n");
  }
  else {
  PRINT_TEST_CASE_MSG("meshlink_join failed to report error accordingly when NULL is passed as invite argument\n");
  }

  meshlink_close(mesh_handle);
  meshlink_destroy("joinconf");
  return !ret;
}

/* Execute join Test Case # 1 - valid case*/
void test_case_meshlink_join_04(void **state) {
    execute_test(test_meshlink_join_04, state);
    return;
}

bool test_meshlink_join_04(void) {
  join_status = false;
  bool ret;

  meshlink_destroy("joinconf");
  char *invite_nut = invite_in_container("relay", NUT_NODE_NAME);
  node_sim_in_container("relay", "1", NULL);

    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("joinconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, status_callback);

    sleep(2);
    /*Joining the NUT with relay*/
    ret = meshlink_join(mesh_handle, invite_nut);
    assert(ret);
    PRINT_TEST_CASE_MSG("meshlink_join status[1st time]: %s\n", meshlink_strerror(meshlink_errno));
    PRINT_TEST_CASE_MSG("NUT joined for the 1st time\n");
    ret = meshlink_join(mesh_handle, invite_nut);
    PRINT_TEST_CASE_MSG("meshlink_join status[2nd time]: %s\n", meshlink_strerror(meshlink_errno));
    assert(meshlink_start(mesh_handle));

    sleep(2);

    if (ret) {
    PRINT_TEST_CASE_MSG("When NUT joined for the 2nd time meshlink_join returned true\n");
    }
    else {
    PRINT_TEST_CASE_MSG("When NUT joined for the 2nd time meshlink_join returned false\n");
    }
    free(invite_nut);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
  meshlink_destroy("joinconf");
  sleep(1);
    return !ret;
}

/* Execute join Test Case # 1 - valid case*/
void test_case_meshlink_join_05(void **state) {
    execute_test(test_meshlink_join_05, state);
    return;
}


bool test_meshlink_join_05(void) {
  join_status = false;
  bool ret ;

  meshlink_destroy("joinconf");
  char *invite_nut = invite_in_container("relay", NUT_NODE_NAME);
  node_sim_in_container("relay", "1", NULL);

    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("joinconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, status_callback);

    PRINT_TEST_CASE_MSG("starting mesh befor joining\n");
    assert(meshlink_start(mesh_handle));

    sleep(2);
    /*Joining the NUT with relay*/
   ret = meshlink_join(mesh_handle, invite_nut);
    PRINT_TEST_CASE_MSG("meshlink_join status: %s\n", meshlink_strerror(meshlink_errno));

    sleep(2);

    if (join_status) {
    PRINT_TEST_CASE_MSG("NUT joined with relay even after starting the mesh\n");
    }
    else {
    PRINT_TEST_CASE_MSG("NUT didn't join with relay even after starting the mesh\n");
    }
    free(invite_nut);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
  meshlink_destroy("joinconf");
  sleep(1);
    return join_status && ret;
}

