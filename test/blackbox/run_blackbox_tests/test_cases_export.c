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
#include "test_cases_export.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/* Execute invite Test Case # 1 - valid case*/
void test_case_export_01(void **state) {
    execute_test(test_export_01, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_export_01(void) {
    meshlink_destroy("export01conf");
    fprintf(stderr, "[ export 01 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("export01conf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    fprintf(stderr, "[ export 01 ] Calling export API\n");
    char *expo = meshlink_export(mesh_handle);

    if (NULL == expo) {
     fprintf(stderr, "[ export 01 ] failed to export meshlink data\n");
       return false;
    }
     fprintf(stderr, "[ export 01 ] Exported meshlink data\n");
       return true;
}

void test_case_export_02(void **state) {
    execute_test(test_export_02, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_export_02(void) {
    fprintf(stderr, "[ export 02 ] Calling export API with NULL mesh handle\n");
    char *expo = meshlink_export(mesh_handle);

    if (NULL == expo) {
     fprintf(stderr, "[ export 02 ] Export API successfully reported NULL\n");
       return true;
    }
     fprintf(stderr, "[ export 02 ] Export API failed to report meshlink handle arg NULL\n");
       return true;
}


void test_case_export_03(void **state) {
    execute_test(test_export_03, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_export_03(void) {
    meshlink_destroy("export03conf");
    fprintf(stderr, "[ export 03 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("export03conf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    fprintf(stderr, "[ export 03 ] Calling export API\n");
    char *expo = meshlink_export(mesh_handle);

    if (NULL == expo) {
     fprintf(stderr, "[ export 03 ] failed to export meshlink data after closing mesh\n");
       return true;
    }
     fprintf(stderr, "[ export 03 ] Exported meshlink data after closing mesh\n");
       return false;
}

void test_case_export_04(void **state) {
    execute_test(test_export_04, state);
    return;
}

/*
    Test Steps:
    1. Run NUT, sleep for a second
    2. Inviting a node

    Expected Result:
    Generates an invitation
*/
bool test_export_04(void) {
    meshlink_destroy("export04conf");
    fprintf(stderr, "[ export 04 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("export04conf", "nut", "node_sim", 1);
    fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    assert(meshlink_destroy("export04conf"));
    fprintf(stderr, "[ export 04 ] Calling export API\n");
    char *expo = meshlink_export(mesh_handle);

    if (NULL == expo) {
     fprintf(stderr, "[ export 04 ] failed to export meshlink data after destroying mesh\n");
       return true;
    }
     fprintf(stderr, "[ export 04 ] Exported meshlink data after destroying mesh\n");
       return false;
}
