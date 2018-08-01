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
#include "test_cases_discovery.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

static bool join_status;
static void status_callback(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
   fprintf(stderr, "In status callback\n");
   if (reach) {
     fprintf(stdout, "[ %s ] node reachable\n", source->name);
   }
   else {
     fprintf(stdout, "[ %s ] node not reachable\n", source->name) ;
   }
   join_status = true;

   return;
}

/* Execute meshlink_discovery Test Case # 1 - connection with relay after being off-line*/
void test_case_discovery_01(void **state) {
    execute_test(test_steps_discovery_01, state);
    return;
}

bool test_steps_discovery_01(void) {
  join_status = false;
  bool ret ;

  meshlink_destroy("discconf");
  char *invite_nut = invite_in_container("relay", NUT_NODE_NAME);
  node_sim_in_container("relay", "1", NULL);

    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("discconf", "nut", "node_sim", 1);
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
  meshlink_destroy("discconf");
    return join_status && ret;
}

/* Execute Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third node */
void test_case_discovery_02(void **state) {
    execute_test(test_steps_discovery_02, state);
    return;
}

/* Test Steps for Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third (relay) node

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, terminate the peer node's running instance
    3. After peer becomes unreachable, wait 60 seconds then re-start the peer node's instance

    Expected Result:
    NUT is re-connected to peer
*/
bool test_steps_discovery_02(void) {
    char  *invite_nut;
    join_status = false;

    meshlink_destroy("discconf02");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    printf("Please turn your interface internet down in 10 seconds\n");
    sleep(10);
    fprintf(stderr, "[discovery 01]enabling discovery\n");
    meshlink_enable_discovery(mesh_handle, false);

    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("discconf02", "nut", "node_sim", 1);
    fprintf(stderr, "[discovery 01]meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, status_callback);
    meshlink_join(mesh_handle, invite_nut);
    assert(meshlink_start(mesh_handle));
    /*starting relay in container*/
    node_sim_in_container("relay", "1", NULL);
    fprintf(stderr, "[discovery 01]Waiting for peer to be connected when discovery being enabled\n");
    sleep(2);

    free(invite_nut);

    if (join_status) {
      fprintf(stderr, "[discovery 01]NUT and relay discovered each other when internet is off and discovery was enabled\n");
    }
    else {
      fprintf(stderr, "[discovery 01]NUT and relay didn't discovered each other when internet is off and discovery was enabled\n");
    }

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("testconf");
    return join_status;
}

/* Execute Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third node */
void test_case_discovery_03(void **state) {
    execute_test(test_steps_discovery_03, state);
    return;
}

/* Test Steps for Meta-connections Test Case # 1 - re-connection to peer after disconnection when
    connected via a third (relay) node

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, terminate the peer node's running instance
    3. After peer becomes unreachable, wait 60 seconds then re-start the peer node's instance

    Expected Result:
    NUT is re-connected to peer
*/
bool test_steps_discovery_03(void) {
    fprintf(stderr, "[test_steps_discovery_03] Passing NULL: as meshlink_enable_discovery mesh argument\n");
     meshlink_enable_discovery(NULL, true);
     if (meshlink_errno == MESHLINK_EINVAL)
        return true;
    else
        return false;
}
