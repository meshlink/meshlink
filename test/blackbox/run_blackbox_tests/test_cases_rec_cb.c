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
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "test_cases_rec_cb.h"
#include <string.h>

static bool call;

static void rec_cb(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
   fprintf(stderr, "In receive callback\n");
   fprintf(stderr, "Received message : %s\n", (char *)data);
   call = true;
}

void test_case_set_rec_cb_01(void **state) {
    execute_test(test_set_rec_cb_01, state);
    return;
}

bool test_set_rec_cb_01(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("setreccb01conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    fprintf(stderr, "[ rec_cd 01] Setting Valid callback\n");
    meshlink_set_receive_cb(mesh_handle, rec_cb);
    assert(meshlink_start(mesh_handle));
        sleep(2);
    fprintf(stderr, "Sending Message\n");
    call = false;
    char *data = "Test message";

    meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);
    assert(node_handle);
    assert(meshlink_send(mesh_handle, node_handle, data, strlen(data) + 1));
    sleep(2);
    if (call) {
      fprintf(stderr, "[ rec_cb 01 ]Invoked callback\n");
    }
    else {
      fprintf(stderr, "No callback invoked\n");
    }
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);

    return call;
}


void test_case_set_rec_cb_02(void **state) {
    execute_test(test_set_rec_cb_02, state);
    return;
}

bool test_set_rec_cb_02(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("setreccb02conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    fprintf(stderr, "[ rec_cd 02] Setting Valid callback\n");
    meshlink_set_receive_cb(mesh_handle, rec_cb);

    assert(meshlink_start(mesh_handle));
    fprintf(stderr, "Sending Message\n");
    call = false;
    char *data = "Test message";
    meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);
    assert(node_handle);
    sleep(2);
    assert(meshlink_send(mesh_handle, node_handle, data, strlen(data) + 1));
    sleep(2);
    if (call) {
      fprintf(stderr, "[ rec_cb 02 ]Invoked callback before disabling\n");
    }
    else {
      fprintf(stderr, "No callback invoked\n");
      return false;
    }
    meshlink_stop(mesh_handle);


    fprintf(stderr, "Setting NULL callback\n");
    meshlink_set_receive_cb(mesh_handle, NULL);

    assert(meshlink_start(mesh_handle));
    fprintf(stderr, "Sending Message\n");
    call = false;
    assert(meshlink_send(mesh_handle, node_handle, data, strlen(data) + 1));
    sleep(5);
    if (call) {
      fprintf(stderr, "Invoked callback even after disabling\n");
    }
    else {
      fprintf(stderr, "No callback invoked after disabling\n");
    }

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return !call;
}




/* Execute Meta-connections Test Case # 3 - re-connection to peer via third node
    after changing IP of peer */
void test_case_set_rec_cb_03(void **state) {
    execute_test(test_set_rec_cb_03, state);
    return;
}

/* Test Steps for Meta-connections Test Case # 3 - re-connection to peer via third node
    after changing IP of peer

    Test Steps:
    1. Run NUT, relay and peer nodes with relay inviting the other two nodes
    2. After connection to peer, change the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
bool test_set_rec_cb_03(void) {
    meshlink_errno_t meshlink_errno_buff = meshlink_errno;
    meshlink_set_receive_cb(NULL, rec_cb);
    assert( meshlink_errno_buff != meshlink_errno );
    if ( meshlink_errno_buff != meshlink_errno ) {
        return true;
    }
    else {
        return false;
    }
}




/* Execute Meta-connections Test Case # 4 - re-connection to peer after changing IP of
    NUT and peer */
void test_case_set_rec_cb_04(void **state) {
    execute_test(test_set_rec_cb_04, state);
    return;
}

/* Execute receive Test Case # 4 - re-connection to peer after changing IP of
    NUT and peer

    Test Steps:
    1. Run NUT and peer nodes with NUT inviting the peer node
    2. After connection to peer, change the NUT's IP Address and the peer node's IP Address

    Expected Result:
    NUT is first disconnected from peer then automatically re-connected to peer
*/
bool test_set_rec_cb_04(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("setreccb04conf", "nut", "node_sim", 1);
    assert(mesh_handle);
    assert(meshlink_start(mesh_handle));

    sleep(2);
    fprintf(stderr, "[ rec_cd 4] Setting Valid callback after starting mesh\n");
    meshlink_set_receive_cb(mesh_handle, rec_cb);

    sleep(2);
    fprintf(stderr, "Sending Message\n");
    call = false;
    char *data = "Test message";
    meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);
    assert(node_handle);
    assert(meshlink_send(mesh_handle, node_handle, data, strlen(data) + 1));
    sleep(2);
    if (call) {
      fprintf(stderr, "[ rec_cb 04 ]Invoked callback\n");
    }
    else {
      fprintf(stderr, "No callback invoked\n");
    }
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);

    return call;
}

