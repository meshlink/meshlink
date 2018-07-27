/*
    test_cases_add_ex_addr.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty o
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "execute_tests.h"
#include "test_cases_channel_get_flags.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/* Execute meshlink_channel_open_ex Test Case # 1 */
void test_case_channel_get_flags_01(void **state) {
    execute_test(test_steps_channel_get_flags_01, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 1*/
bool test_steps_channel_get_flags_01(void) {
    bool result = false;
    fprintf(stderr, "[ channel get flags 01 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    fprintf(stderr, "[ channel get flags 01 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    fprintf(stderr, "[ channel get flags 01 ] Opening TCP channel ex\n");
    meshlink_channel_t *channel = node->priv;
    channel = meshlink_channel_open_ex(mesh_handle, node, 50000, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
    assert(channel != NULL);
    fprintf(stderr, "meshlink_channel_open_ex has used TCP semantics\n");
    sleep(1);

    unsigned int flags = meshlink_channel_get_flags(mesh_handle, channel);
    if (flags == MESHLINK_CHANNEL_TCP) {
    fprintf(stderr, "[ channel get flags 01 ] meshlink_channel_get_flags obtained flags correctly");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
    }
    else {
     meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
    }
}

/* Execute meshlink_channel_get flags Test Case # 2 */
void test_case_channel_get_flags_02(void **state) {
    execute_test(test_steps_channel_get_flags_02, state);
    return;
}

/* Test Steps for meshlink_channel_get flags Test Case # 2*/
bool test_steps_channel_get_flags_02(void) {
    bool result = false;
    fprintf(stderr, "[ channel get flags 02 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    fprintf(stderr, "[ channel get flags 02 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    fprintf(stderr, "[ channel get flags 02 ] Opening TCP channel ex\n");
    meshlink_channel_t *channel = node->priv;
    channel = meshlink_channel_open_ex(mesh_handle, node, 50000, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
    assert(channel != NULL);
    fprintf(stderr, "meshlink_channel_open_ex has used TCP semantics\n");
    sleep(1);

    unsigned int flags = meshlink_channel_get_flags(NULL, channel);
    if (meshlink_errno == MESHLINK_EINVAL) {
    fprintf(stderr, "[ channel get flags 02 ] Reported error correctly when NULL is passed as argument");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
    }
    else {
     meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
    }
}

/* Execute meshlink_channel_get flags Test Case # 3 */
void test_case_channel_get_flags_03(void **state) {
    execute_test(test_steps_channel_get_flags_03, state);
    return;
}

/* Test Steps for meshlink_channel_get flags Test Case # 2*/
bool test_steps_channel_get_flags_03(void) {
    bool result = false;
    fprintf(stderr, "[ channel get flags 03 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    fprintf(stderr, "[ channel get flags 03 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    fprintf(stderr, "[ channel get flags 03 ] Opening TCP channel ex\n");
    meshlink_channel_t *channel = node->priv;
    channel = meshlink_channel_open_ex(mesh_handle, node, 50000, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
    assert(channel != NULL);
    fprintf(stderr, "meshlink_channel_open_ex has used TCP semantics\n");
    sleep(1);

    unsigned int flags = meshlink_channel_get_flags(NULL, channel);
    if (meshlink_errno == MESHLINK_EINVAL) {
    fprintf(stderr, "[ channel get flags 03 ] Reported error correctly when NULL is passed as argument");
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
    }
    else {
     meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
    }
}

/* Execute meshlink_channel_get flags Test Case # 4 */
void test_case_channel_get_flags_04(void **state) {
    execute_test(test_steps_channel_get_flags_04, state);
    return;
}

/* Test Steps for meshlink_channel_get flags Test Case # 4*/
bool test_steps_channel_get_flags_04(void) {
    fprintf(stderr, "[ channel get flags 04 ] Opening NUT\n");
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("channelexconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
    fprintf(stderr, "[ channel get flags 04 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    fprintf(stderr, "[ channel get flags 04 ] Opening TCP channel ex\n");
    meshlink_channel_t *channel = node->priv;
    channel = meshlink_channel_open_ex(mesh_handle, node, 50000, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
    assert(channel != NULL);
    fprintf(stderr, "meshlink_channel_open_ex has used TCP semantics\n");
    sleep(1);

    fprintf(stderr, "[ channel get flags 04 ] closing channel\n");
    meshlink_channel_close(mesh_handle, channel);

    unsigned int flags = meshlink_channel_get_flags(mesh_handle, channel);
    if (flags == MESHLINK_CHANNEL_TCP) {
    fprintf(stderr, "[ channel get flags 04 ] Obtained flags even after closing channel");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return true;
    }
    else {
     meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    return false;
    }
}
