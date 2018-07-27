/*
    test_cases_add_ex_addr.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_ex.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
#define CHAT_PORT 8000
static bool channel_rec;
static int channel_rec_len;
static void cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
  char *data = (char *) dat;
  fprintf(stderr, "Invoked channel Receive callback\n");
  fprintf(stderr, "Received message is : %s\n", data);
  channel_rec_len = len;
  channel_rec = true;
}

static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)data;
	(void)len;

	channel_rec = true;

	fprintf(stderr, "Accepted incoming channel from '%s'\n", channel->node->name);

	// Remember the channel
	channel->node->priv = channel;

	// Set the receive callback
	meshlink_set_channel_receive_cb(mesh, channel, cb);

	// Accept this channel
	return true;
}


/* Execute meshlink_channel_open_ex Test Case # 1 */
void test_case_channel_ex_01(void **state) {
    execute_test(test_steps_channel_ex_01, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 1*/
bool test_steps_channel_ex_01(void) {
    bool result = false;
    fprintf(stderr, "[ channel open ex 01 ] Opening NUT\n");
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
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);
    fprintf(stderr, "[ channel open ex 01 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    char string[100] = "Test the 1st case";
    channel_rec = false;
    fprintf(stderr, "[ channel open ex 01 ] Opening TCP channel ex\n");
    meshlink_channel_t *channel = node->priv;
    channel = meshlink_channel_open_ex(mesh_handle, node, CHAT_PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
    fprintf(stderr, "meshlink_channel_open_ex with send queue has shown CORE DUMP\n");
    sleep(1);
    meshlink_destroy("channelexconf");
    return false;
}

/* Execute meshlink_channel_open_ex Test Case # 2 */
void test_case_channel_ex_02(void **state) {
    execute_test(test_steps_channel_ex_02, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 2*/
bool test_steps_channel_ex_02(void) {
    bool result = false;
    fprintf(stderr, "[ channel open ex 02 ] Opening NUT\n");
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
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);
    fprintf(stderr, "[ channel open ex 02 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    channel_rec = false;
    //sleep(1);
    fprintf(stderr, "[ channel open ex 02 ] Opening UDP channel ex\n");
    meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, node, 8001, cb, NULL, 0, MESHLINK_CHANNEL_UDP);
    assert(channel != NULL);

    if (channel != NULL) {
      fprintf(stderr, "[ channel open ex 02 ] Channel created and invoked receive call back\n");
      result = true;
    }
    else {
      fprintf(stderr, "[ channel open ex 02 ] Channel didn't create and invoke receive call back\n");
      result = false;
    }
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");

    return result;
}

/* Execute meshlink_channel_open_ex Test Case # 3 */
void test_case_channel_ex_03(void **state) {
    execute_test(test_steps_channel_ex_03, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 2*/
bool test_steps_channel_ex_03(void) {
    bool result = false;
    fprintf(stderr, "[ channel open ex 03 ] Opening NUT\n");
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
    fprintf(stderr, "[ channel open ex 03 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    channel_rec = false;
    fprintf(stderr, "[ channel open ex 03 ] Opening TCP channel ex with no send queue \n");
    meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, node, CHAT_PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
    assert(channel != NULL);

    if (channel != NULL) {
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

/* Execute meshlink_channel_open_ex Test Case # 4 */
void test_case_channel_ex_04(void **state) {
    execute_test(test_steps_channel_ex_04, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 2*/
bool test_steps_channel_ex_04(void) {
    bool result = false;
    fprintf(stderr, "[ channel open ex 04 ] Opening NUT\n");
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
    fprintf(stderr, "[ channel open ex 04 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    channel_rec = false;
    fprintf(stderr, "[ channel open ex 04 ] Opening TCP channel ex with no receive callback \n");
    meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, node, CHAT_PORT, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
    assert(channel != NULL);

    if (channel != NULL) {
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

/* Execute meshlink_channel_open_ex Test Case # 5 */
void test_case_channel_ex_05(void **state) {
    execute_test(test_steps_channel_ex_05, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 5*/
bool test_steps_channel_ex_05(void) {
    bool result = false;
    fprintf(stderr, "[ channel open ex 05 ] Opening NUT\n");
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
    fprintf(stderr, "[ channel open ex 05 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    channel_rec = false;
    fprintf(stderr, "[ channel open ex 05 ] Trying to open channel using mesh handle as NULL argument \n");
    meshlink_channel_t *channel = meshlink_channel_open_ex(NULL, node, CHAT_PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
    assert(channel == NULL);

    if (channel == NULL) {
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    fprintf(stderr, "[ channel open ex 05 ] Error reported correctly \n");
    return true;
    }
    else {
     meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    fprintf(stderr, "[ channel open ex 05 ] Failed to report error \n");
    return false;
    }
}

/* Execute meshlink_channel_open_ex Test Case # 6 */
void test_case_channel_ex_06(void **state) {
    execute_test(test_steps_channel_ex_06, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 6*/
bool test_steps_channel_ex_06(void) {
    bool result = false;
    fprintf(stderr, "[ channel open ex 06 ] Opening NUT\n");
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
    fprintf(stderr, "[ channel open ex 06 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    channel_rec = false;
    fprintf(stderr, "[ channel open ex 06 ] Trying to open channel using node handle as NULL argument \n");
    meshlink_channel_t *channel = meshlink_channel_open_ex(mesh_handle, NULL, 8000, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
    assert(channel == NULL);

    if (channel == NULL) {
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    fprintf(stderr, "[ channel open ex 06 ] Error reported correctly \n");
    return true;
    }
    else {
     meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    fprintf(stderr, "[ channel open ex 06 ] Failed to report error \n");
    return false;
    }
}

/* Execute meshlink_channel_open_ex Test Case # 7 */
void test_case_channel_ex_07(void **state) {
    execute_test(test_steps_channel_ex_07, state);
    return;
}

/* Test Steps for meshlink_channel_open_ex Test Case # 7*/
bool test_steps_channel_ex_07(void) {
    bool result = false;
    fprintf(stderr, "[ channel open ex 07 ] Opening NUT\n");
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
    fprintf(stderr, "[ channel open ex 07 ] starting mesh\n");
    assert(meshlink_start(mesh_handle));

    meshlink_node_t *node = meshlink_get_self(mesh_handle);
    assert(node != NULL);

    channel_rec = false;
    fprintf(stderr, "[ channel open ex 07 ] Trying to open channel using invalid flag argument \n");
    meshlink_channel_t *channel = meshlink_channel_open_ex(NULL, node, 8000, cb, NULL, 0, 1000);

    if (channel == NULL) {
    meshlink_channel_close(mesh_handle, channel);
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    fprintf(stderr, "[ channel open ex 07 ] Error reported correctly \n");
    return true;
    }
    else {
     meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelexconf");
    fprintf(stderr, "[ channel open ex 07 ] Failed to report error \n");
    return false;
    }
}
