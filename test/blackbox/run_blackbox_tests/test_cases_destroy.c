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
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG


/* Execute destroy Test Case # 1 - valid case*/
void test_case_meshlink_destroy_01(void **state) {
    execute_test(test_meshlink_destroy_01, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Stop and Close NUT, and destroy the confbase

    Expected Result:
    confbase should be deleted
*/
bool test_meshlink_destroy_01(void) {
    bool result = false;
    fprintf(stderr, "[ destroy 01 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("destroy01conf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    assert(meshlink_start(mesh_handle));

    sleep(1);

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    fprintf(stderr, "[ destroy 01] Destroying NUT's confbase\n");
    result = meshlink_destroy("destroy01conf");

    return result;
}



/* Execute destroy Test Case # 2 - passing NULL argument to the API */
void test_case_meshlink_destroy_02(void **state) {
    execute_test(test_meshlink_destroy_02, state);
    return;
}

/*
    Test Steps:
    1. Just passing NULL as argument to the API

    Expected Result:
    Return false reporting failure
*/
bool test_meshlink_destroy_02(void) {
    bool result = false;

    PRINT_TEST_CASE_MSG("Passing NULL as an argument to meshlink_destroy\n");

    result = meshlink_destroy(NULL);

    return !result;
}




/* Execute status Test Case # 3 - destroying non existing file */
void test_case_meshlink_destroy_03(void **state) {
    execute_test(test_meshlink_destroy_03, state);
    return;
}

/*

    Test Steps:
    1. unlink if there's any such test file
    2. Call API with that file name

    Expected Result:
    Return false reporting failure
*/
bool test_meshlink_destroy_03(void) {
    bool result = false;

    unlink("non_existing_file");

    PRINT_TEST_CASE_MSG("Passing non-existing file as an argument to meshlink_destroy\n");

    result = meshlink_destroy("non_existing_file");

    return !result;
}




/* Execute destroy Test Case # 4 - destroying bad file i.e other than current mesh confbase file */
void test_case_meshlink_destroy_04(void **state) {
    execute_test(test_meshlink_destroy_04, state);
    return;
}

/*
    Test Steps:
    1. Creating a new file
    2. Passing as an argument to this API

    Expected Result:
    Return false reporting failure
*/
bool test_meshlink_destroy_04(void) {
    bool result = false;

    mkdir("badconf", 0777);

    result = meshlink_destroy("badconf");

    return !result;
}




/* Execute destroy Test Case # 5 - destroying before stoping the mesh */
void test_case_meshlink_destroy_05(void **state) {
    execute_test(test_meshlink_destroy_05, state);
    return;
}
// REMOVING TEST CASE 05 since it is obvious that destroy is not going to destroy w.r.t
// the intended confbase i.e, mesh handle argument should be added.

/*
    Test Steps:
    1. Run NUT
    2. destroy the confbase before stoping it

    Expected Result:
    Return false reporting failure that it cannot delete the file
*/
bool test_meshlink_destroy_05(void) {
    bool result = false;
    fprintf(stderr, "[ destroy 01 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("destroy05conf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    assert(meshlink_start(mesh_handle));

    sleep(1);

    PRINT_TEST_CASE_MSG("Started NUT\n");
    result = meshlink_destroy("testconf");

    return !result;
}
