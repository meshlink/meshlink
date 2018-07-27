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
#include "test_cases_status_cb.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG


static bool status;
static void status_callback(meshlink_handle_t *mesh, meshlink_node_t *source, bool reach) {
   fprintf(stderr, "In status callback\n");
   if (reach) {
     fprintf(stdout, "[ %s ] node reachable\n", source->name);
   }
   else {
     fprintf(stdout, "[ %s ] node not reachable\n", source->name) ;
   }
   status = true;

   return;
}


/* Execute status callback Test Case # 1 - valid case */
void test_case_set_status_cb_01(void **state) {
    execute_test(test_set_status_cb_01, state);
    return;
}

bool test_set_status_cb_01(void) {
    char *invite_peer, *invite_nut;
    status = false;
    int i;

    meshlink_destroy("testconf");
    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("testconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, status_callback);
    meshlink_join(mesh_handle, invite_nut);
    assert(meshlink_start(mesh_handle));
    PRINT_TEST_CASE_MSG("Waiting 60 sec for peer to be re-connected\n");
    for(i = 0; i < 60; i++) {
        if(meta_conn_status[1]) {
            break;
        }
        sleep(1);
    }

    free(invite_peer);
    free(invite_nut);

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return status;
}

/* Execute status callback Test Case # 2 - Invalid case */
void test_case_set_status_cb_02(void **state) {
    execute_test(test_set_status_cb_02, state);
    return;
}

bool test_set_status_cb_02(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("setstat02conf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    meshlink_errno_t meshlink_errno_buff = meshlink_errno;
    fprintf(stderr, "[ status 02 ]Setting callback API with mesh handle as NULL\n");
    meshlink_set_node_status_cb(NULL, status_callback);
    if ( MESHLINK_EINVAL == meshlink_errno ) {
       fprintf(stderr, "[ status 02 ]Setting callback API with mesh handle as NULL reported SUCCESSFULY\n");
       return true;
    }
    else {
       fprintf(stderr, "[ status 02 ]API with mesh handle as NULL failured to report error\n");
       return false;
    }
}

/* Execute status callback Test Case # 3 - setting callback after starting mesh */
void test_case_set_status_cb_03(void **state) {
    execute_test(test_set_status_cb_03, state);
    return;
}

bool test_set_status_cb_03(void) {
    char *invite_peer, *invite_nut;
    status = false;
    int i;

    meshlink_destroy("testconf");
    invite_peer = invite_in_container("relay", "peer");
    invite_nut = invite_in_container("relay", NUT_NODE_NAME);
    node_sim_in_container("relay", "1", NULL);
    node_sim_in_container("peer", "1", invite_peer);
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("testconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    meshlink_join(mesh_handle, invite_nut);
    meshlink_start(mesh_handle);

    /* Set up callback for node status after starting (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, status_callback);

    PRINT_TEST_CASE_MSG("Waiting 60 sec for peer to be re-connected\n");
    for(i = 0; i < 60; i++) {
        if(meta_conn_status[1]) {
            break;
        }
        sleep(1);
    }

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    free(invite_peer);
    free(invite_nut);

    return status;
}

