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
#include "test_cases_get_fingerprint.h"
#include <string.h>

/* Execute get_fingerprint Test Case # 1 - Valid Case of obtaing publickey of a node in a
   mesh */
void test_case_get_fingerprint_cb_01(void **state) {
    execute_test(test_get_fingerprint_cb_01, state);
    return;
}

bool test_get_fingerprint_cb_01(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("getfingerprint01conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));
    fprintf(stderr, "[ get_finger 01 ]Calling get fingerprint for NUT\n");

    meshlink_node_t *source = meshlink_get_self(mesh_handle);
    assert(source);
    char *fp = meshlink_get_fingerprint(mesh_handle, source);

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);

    return fp;
}

/* Execute get_fingerprint Test Case # 2 - Invalid Case - trying t0 obtain publickey of a node in a
   mesh by passing NULL*/
void test_case_get_fingerprint_cb_02(void **state) {
    execute_test(test_get_fingerprint_cb_02, state);
    return;
}

bool test_get_fingerprint_cb_02(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("getfingerprint02conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);

    fprintf(stderr, "[ get_finger 02 ]Calling get fingerprint for NUT\n");
    char *fp = meshlink_get_fingerprint(NULL, node_handle);
    fprintf(stderr, "meshlink_get_fingerprint status: %s\n", meshlink_strerror(meshlink_errno));

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);

    if (fp)
      return false;
    else
      return true;
}

/* Execute get_fingerprint Test Case # 3 - Invalid Case- trying to obtain publickey of a node
   mesh */
void test_case_get_fingerprint_cb_03(void **state) {
    execute_test(test_get_fingerprint_cb_03, state);
    return;
}

bool test_get_fingerprint_cb_03(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("getfingerprint03conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));

    fprintf(stderr, "[ get_finger 03 ]Calling get fingerprint for NUT\n");
    char *fp = meshlink_get_fingerprint(mesh_handle, NULL);
    fprintf(stderr, "meshlink_get_fingerprint status: %s\n", meshlink_strerror(meshlink_errno));

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);

    if (fp)
      return false;
    else
      return true;
}

/* Execute get_fingerprint Test Case # 4 - Functionality test- trying to obtain publickey of a node
   after stopping a mesh */
void test_case_get_fingerprint_cb_04(void **state) {
    execute_test(test_get_fingerprint_cb_04, state);
    return;
}

bool test_get_fingerprint_cb_04(void) {
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("getfingerprint04conf", "nut", "node_sim", 1);
    assert(mesh_handle);

    assert(meshlink_start(mesh_handle));
    meshlink_node_t *node_handle = meshlink_get_self(mesh_handle);

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);

    fprintf(stderr, "[ get_finger 04 ]Calling get fingerprint for NUT after stopping\n");
    char *fp = meshlink_get_fingerprint(mesh_handle, node_handle);
    fprintf(stderr, "meshlink_get_fingerprint status: %s\n", meshlink_strerror(meshlink_errno));

    if (fp)
      return false;
    else
      return true;
}


