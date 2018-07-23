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
#include "test_cases.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include "test_cases_get_all_nodes.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>

/* Execute get_all_nodes Test Case # 2 - Invalid case - get all nodes in the mesh passing NULL */
void test_case_get_all_nodes_02(void **state) {
    execute_test(test_get_all_nodes_02, state);
    return;
}

bool test_get_all_nodes_02(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("getallnodes02conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    size_t nmemb = 0;
    meshlink_node_t **node = meshlink_get_all_nodes(NULL, NULL, &nmemb);
   if (!node) {
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    fprintf(stderr, "meshlink_get_all_nodes: %s\n", meshlink_strerror(meshlink_errno));
    fprintf(stderr, "[ get_all_nodes 02 ]get all nodes API successfuly returned failure on passing NULL as mesh handle arg\n");
    return true;
    }
    else {
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    fprintf(stderr, "meshlink_get_all_nodes: %s\n", meshlink_strerror(meshlink_errno));
    fprintf(stderr, "[ get_all_nodes 02 ]get all nodes API didnt report failure on passing NULL as mesh handle arg\n");
    return false;
    }

}

/* Execute get_all_nodes Test Case # 1 - Valid case - get all nodes in the mesh */
void test_case_get_all_nodes_01(void **state) {
    execute_test(test_get_all_nodes_01, state);
    return;
}

bool test_get_all_nodes_01(void) {
    int i;
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("getallnodes01conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));


    size_t nmemb = 0, nmemb2 = 0;
    meshlink_node_t **nodes = meshlink_get_all_nodes(mesh_handle, NULL, &nmemb);

   if (!nodes) {
    fprintf(stderr, "[ get_all_nodes 01 ]Failed to get nodes\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return false;
   }

   if(!nmemb) {
    fprintf(stderr, "[ get_all_nodes 01 ]Failed to get nodes\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return false;
   }

//    node_sim_in_container("peer", "1", invite_peer);
//    PRINT_TEST_CASE_MSG("Waiting for peer to be connected\n");
    /* TO DO: Implement this with a timeout */
//    while(!meta_conn_status[0])
//        sleep(1);
    meshlink_stop(mesh_handle);

   nodes = meshlink_get_all_nodes(mesh_handle, NULL, &nmemb);

   if (!nodes) {
    fprintf(stderr, "[ get_all_nodes 01 ]Failed to realloc by get_nodes\n");
    meshlink_close(mesh_handle);
    return false;
   }

   if(!nmemb2 || (nmemb2 < nmemb)) {
    fprintf(stderr, "[ get_all_nodes 01 ]Failed to realloc by get_nodes\n");
    meshlink_close(mesh_handle);
    return false;
   }

    meshlink_close(mesh_handle);
    fprintf(stderr, "[ get_all_nodes 01 ]get all nodes API success\n");
    return true;
}

/* Execute get_all_nodes Test Case # 3 - Invalid case - get all nodes in the mesh passing NULL as nmeb arg */
void test_case_get_all_nodes_03(void **state) {
    execute_test(test_get_all_nodes_03, state);
    return;
}

bool test_get_all_nodes_03(void) {
    int i;
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("getallnodes03conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    meshlink_node_t **nodes = meshlink_get_all_nodes(mesh_handle, NULL, NULL);

   if (!nodes) {
    fprintf(stderr, "[ get_all_nodes 03 ]get_all_nodes Reported failure succesfully\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return true;
   }

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return false;

}
