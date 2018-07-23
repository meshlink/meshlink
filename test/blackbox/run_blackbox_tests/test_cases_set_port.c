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
#include "test_cases_destroy.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include "test_cases_set_port.h"
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/* Execute destroy Test Case # 1 - valid case*/
void test_case_set_port_01(void **state) {
    execute_test(test_set_port_01, state);
    return;
}

bool test_set_port_01(void) {
    fprintf(stderr, "[ set_port 01 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("setport01conf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

    bool ret = meshlink_set_port(mesh_handle, 8000);

    return ret;
}

/* Execute destroy Test Case # 1 - valid case*/
void test_case_set_port_02(void **state) {
    execute_test(test_set_port_02, state);
    return;
}


bool test_set_port_02(void) {
    fprintf(stderr, "[ set_port 02 ]Setting NULL as mesh handle\n");

    bool ret = meshlink_set_port(NULL, 8000);

    if ((MESHLINK_EINVAL == meshlink_errno) && (false == ret))  {
      fprintf(stderr, "[ set_port 02 ]NULL argument reported SUCCESSFULY\n");
       return true;
    }

    fprintf(stderr, "[ set_port 02 ]failed to report NULL argument\n");
    return false;
}


void test_case_set_port_03(void **state) {
    execute_test(test_set_port_03, state);
    return;
}


bool test_set_port_03(void) {
    fprintf(stderr, "[ set_port 03 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("getport03conf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

    fprintf(stderr, "[ set_port 03 ]Setting Invalid port argument\n");
    bool ret = meshlink_set_port(mesh_handle, -1);

    if ((MESHLINK_EINVAL != meshlink_errno) && (false != ret))  {
    fprintf(stderr, "[ set_port 03 ]failed to report Invalid port argument\n");
    return false;
    }

    fprintf(stderr, "[ set_port 03 ]Setting Invalid port argument\n");

    ret = meshlink_set_port(mesh_handle, 100000);

    if ((MESHLINK_EINVAL != meshlink_errno) && (false != ret))  {
    fprintf(stderr, "[ set_port 03 ]failed to report Invalid port argument\n");
    return false;
    }

      fprintf(stderr, "[ set_port 03 ]Invalid port argument reported SUCCESSFULY\n");
       return true;
}

/* Execute destroy Test Case # 1 - valid case*/
void test_case_set_port_04(void **state) {
    execute_test(test_set_port_04, state);
    return;
}

bool test_set_port_04(void) {
    fprintf(stderr, "[ set_port 04 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("getport04conf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    fprintf(stderr, "[ set_port 04 ] Starting NUT\n");

    assert(meshlink_start(mesh_handle));

    fprintf(stderr, "[ set_port 04 ] Setting port 50000\n");

    bool ret = meshlink_set_port(mesh_handle, 50000);

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    return !ret;
}

/* Execute destroy Test Case # 1 - valid case*/
void test_case_set_port_05(void **state) {
    execute_test(test_set_port_05, state);
    return;
}

bool test_set_port_05(void) {
    fprintf(stderr, "[ set_port 05 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("getport05conf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

    fprintf(stderr, "[ get_port 05 ] Setting port 80 HTTP reserved port\n");

    bool ret = meshlink_set_port(mesh_handle, 80);

    return !ret;
}

