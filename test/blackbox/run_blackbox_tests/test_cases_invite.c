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
#include "test_cases_invite.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/* Execute invite Test Case # 1 - valid case*/
void test_case_invite_01(void **state) {
    execute_test(test_invite_01, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_invite_01(void) {
    meshlink_destroy("invite01conf");
    fprintf(stderr, "[ invite 01 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("invite01conf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

    fprintf(stderr, "\n[ invite 01 ]Generating INVITATION\n");
    char *invitation = meshlink_invite(mesh_handle, "new");
    if (NULL == invitation) {
      fprintf(stderr, "\n[ invite 01 ]Failed to generate INVITATION\n");
      return false;
    }
      fprintf(stderr, "\n[ invite 01 ]Generated INVITATION successfully\t %s \n", invitation);
      assert(meshlink_destroy("invite01conf"));

    return true;
}


/* Execute invite Test Case # 1 - valid case*/
void test_case_invite_02(void **state) {
    execute_test(test_invite_02, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_invite_02(void) {
    fprintf(stderr, "\n[ invite 02 ]Trying to generate INVITATION by passing NULL as mesh link handle\n");
    char *invitation = meshlink_invite(NULL, "nut");
    if (NULL == invitation && MESHLINK_EINVAL == meshlink_errno) {
      fprintf(stderr, "\n[ invite 02 ]invite API reported error SUCCESSFULLY\n");
      return true;
    }
      fprintf(stderr, "\n[ invite 02 ]Failed to report error\n");

    return false;
}

/* Execute invite Test Case # 1 - valid case*/
void test_case_invite_03(void **state) {
    execute_test(test_invite_03, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_invite_03(void) {
    fprintf(stderr, "[ destroy 03 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("invite03conf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    fprintf(stderr, "\n[ invite 03 ]Trying to generate INVITATION by passing NULL as mesh link handle\n");
    char *invitation = meshlink_invite(mesh_handle, NULL);
    if (NULL == invitation) {
      fprintf(stderr, "\n[ invite 03 ]invite API reported error SUCCESSFULLY\n");
      return true;
    }
      fprintf(stderr, "\n[ invite 03 ]Failed to report error\n");

    return false;
}

void test_case_invite_04(void **state) {
    execute_test(test_invite_04, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_invite_04(void) {
    fprintf(stderr, "[ destroy 04 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("invite04conf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

    assert(meshlink_start(mesh_handle));
    fprintf(stderr, "\n[ invite 04 ]Trying to generate INVITATION after starting meshlink handle\n");
    char *invitation = meshlink_invite(mesh_handle, NULL);
    if (NULL == invitation) {
      fprintf(stderr, "\n[ invite 04 ]invite API reported error SUCCESSFULLY\n");
      return true;
    }
      fprintf(stderr, "\n[ invite 04 ]Failed to report error\n");

    return false;
}


void test_case_invite_05(void **state) {
    execute_test(test_invite_05, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_invite_05(void) {
    fprintf(stderr, "[ destroy 05 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("invite05conf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    fprintf(stderr, "\n[ invite 05 ]Trying to generate INVITATION by \
              passing node name that already exists in mesh\n");
    char *invitation = meshlink_invite(mesh_handle, "nut");
    if (NULL == invitation) {
      fprintf(stderr, "\n[ invite 05 ]invite API reported error SUCCESSFULLY if a node tries to join again\n");
      return true;
    }
      fprintf(stderr, "\n[ invite 05 ]Failed to report error if a node tries to join again\n");

    return false;
}


void test_case_invite_06(void **state) {
    execute_test(test_invite_06, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_invite_06(void) {
    fprintf(stderr, "[ destroy 06 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("invite06conf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    fprintf(stderr, "\n[ invite 06 ]Trying to generate INVITATION more than once\n");
    char *invitation;
    int i ;
    meshlink_set_log_cb(mesh_handle, 0, NULL);
    for (i = 500; i; i--) {
        invitation = meshlink_invite(mesh_handle, "rat");
        if (NULL == invitation) {
           fprintf(stderr, "\n[ invite 06 ]Failed to generate INVITATION at %dth time in the loop\n", i);
           return false;
        }
    }
    fprintf(stderr, "\n[ invite 06 ]Generated 1000 INVITATIONs successfully in a loop\n");
    return true;
}
