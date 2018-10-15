/*
    test_cases_channel_set_poll_cb.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

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
#include "test_cases_channel_set_poll_cb.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
/* Modify this to change the port number */
#define PORT 8000

static void test_case_channel_set_poll_cb_01(void **state);
static bool test_steps_channel_set_poll_cb_01(void);
static void test_case_channel_set_poll_cb_02(void **state);
static bool test_steps_channel_set_poll_cb_02(void);
static void test_case_channel_set_poll_cb_03(void **state);
static bool test_steps_channel_set_poll_cb_03(void);

static black_box_state_t test_case_channel_set_poll_cb_01_state = {
    /* test_case_name = */ "test_case_channel_set_poll_cb_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
static black_box_state_t test_case_channel_set_poll_cb_02_state = {
    /* test_case_name = */ "test_case_channel_set_poll_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
static black_box_state_t test_case_channel_set_poll_cb_03_state = {
    /* test_case_name = */ "test_case_channel_set_poll_cb_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};
/* poll_stat gives access when poll callback call is invoked */
static bool poll_stat;


/* channel receive callback function */
static void cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *dat, size_t len) {
  char *data = (char *) dat;
  fprintf(stderr, "Invoked channel Receive callback\n");
  fprintf(stderr, "Received message is : %s\n", data);
}
/* channel accept callback function */
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)data;
	(void)len;

	fprintf(stderr, "Accepted incoming channel from '%s'\n", channel->node->name);
	// Remember the channel
	channel->node->priv = channel;
	// Set the receive callback
	meshlink_set_channel_receive_cb(mesh, channel, cb);
	// Accept this channel
	return true;
}
/* channel poll callback function */
static void channel_poll(meshlink_handle_t *mesh, meshlink_channel_t *channel, size_t len) {
	fprintf(stderr, "Channel to '%s' connected\n", channel->node->name);
	meshlink_set_channel_poll_cb(mesh, channel, NULL);
	poll_stat = true;
	return;
}

/* Execute meshlink_channel_set_poll_cb Test Case # 1 */
static void test_case_channel_set_poll_cb_01(void **state) {
    execute_test(test_steps_channel_set_poll_cb_01, state);
    return;
}
/* Test Steps for meshlink_channel_set_poll_cb Test Case # 1

    Test Steps:
    1. Run NUT
    2. Open channel of the NUT itself
    3. Send data to NUT

    Expected Result:
    Opens a channel and also invokes poll callback.
*/
static bool test_steps_channel_set_poll_cb_01(void) {
  poll_stat = false;
  meshlink_destroy("channelpollconf");
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("channelpollconf", "nut", "node_sim", 1);
  fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);

  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  PRINT_TEST_CASE_MSG("starting mesh\n");
  assert(meshlink_start(mesh_handle));
  sleep(1);

  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);
  PRINT_TEST_CASE_MSG("Opening TCP channel ex\n");
  meshlink_channel_t *channel = node->priv;
  channel = meshlink_channel_open_ex(mesh_handle, node, 9000, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
  fprintf(stderr, "meshlink_channel_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(channel != NULL);

  PRINT_TEST_CASE_MSG("Setting poll cb\n");
  meshlink_set_channel_poll_cb(mesh_handle, channel, channel_poll);
  sleep(1);

  bool ret = poll_stat;

  meshlink_stop(mesh_handle);
  meshlink_close(mesh_handle);
  meshlink_destroy("channelpollconf");

  if(ret) {
    PRINT_TEST_CASE_MSG("Poll callback invoked\n");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("Poll callback not invoked\n");
    return false;
  }
}

/* Execute meshlink_channel_set_poll_cb Test Case # 2 */
static void test_case_channel_set_poll_cb_02(void **state) {
    execute_test(test_steps_channel_set_poll_cb_02, state);
    return;
}
/* Test Steps for meshlink_channel_set_poll_cb Test Case # 2

    Test Steps:
    1. Run NUT
    2. Open channel of the NUT itself
    3. Pass NULL as mesh handle argument for meshlink_set_channel_poll_cb API

    Expected Result:
    Reports error accordingly by returning NULL
*/
static bool test_steps_channel_set_poll_cb_02(void) {
  fprintf(stderr, "[ channel poll 02 ] Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  mesh_handle = meshlink_open("channelpollconf", "nut", "node_sim", 1);
  PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);

  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);

  PRINT_TEST_CASE_MSG("starting mesh\n");
  assert(meshlink_start(mesh_handle));
  sleep(1);
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  PRINT_TEST_CASE_MSG("Opening TCP channel ex\n");
  meshlink_channel_t *channel = node->priv;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, cb, NULL, 0, MESHLINK_CHANNEL_TCP);
  fprintf(stderr, "meshlink_channel_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(channel != NULL);
  sleep(1);

  PRINT_TEST_CASE_MSG("Setting poll cb\n");
  meshlink_set_channel_poll_cb(NULL, channel, channel_poll);

  if(meshlink_errno == MESHLINK_EINVAL) {
    PRINT_TEST_CASE_MSG("set poll callback reported error successfully when NULL is passed as mesh argument\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelpollconf");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("set poll callback didn't report error when NULL is passed as mesh argument\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelpollconf");
    return false;
  }
}

/* Execute meshlink_channel_set_poll_cb Test Case # 3 */
static void test_case_channel_set_poll_cb_03(void **state) {
    execute_test(test_steps_channel_set_poll_cb_03, state);
    return;
}
/* Test Steps for meshlink_channel_set_poll_cb Test Case # 3

    Test Steps:
    1. Run NUT
    2. Open channel of the NUT itself
    3. Pass NULL as channel handle argument for meshlink_set_channel_poll_cb API

    Expected Result:
    Reports error accordingly by returning NULL
*/
static bool test_steps_channel_set_poll_cb_03(void) {
  PRINT_TEST_CASE_MSG("Opening NUT\n");
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  mesh_handle = meshlink_open("channelpollconf", "nut", "node_sim", 1);
  PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
  assert(mesh_handle);

  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);
  PRINT_TEST_CASE_MSG("starting mesh\n");
  assert(meshlink_start(mesh_handle));

  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);

  PRINT_TEST_CASE_MSG("Setting poll cb\n");
  meshlink_set_channel_poll_cb(mesh_handle, NULL, channel_poll);

  if(meshlink_errno == MESHLINK_EINVAL) {
    PRINT_TEST_CASE_MSG("set poll callback reported error successfully when NULL is passed as mesh argument\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelpollconf");
    return true;
  } else {
    PRINT_TEST_CASE_MSG("set poll callback didn't report error when NULL is passed as mesh argument\n");
    meshlink_stop(mesh_handle);
    meshlink_close(mesh_handle);
    meshlink_destroy("channelpollconf");
    return false;
  }
}


int test_meshlink_set_channel_poll_cb(void) {
  const struct CMUnitTest blackbox_channel_set_poll_cb_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_01, NULL, NULL,
            (void *)&test_case_channel_set_poll_cb_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_02, NULL, NULL,
            (void *)&test_case_channel_set_poll_cb_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_set_poll_cb_03, NULL, NULL,
            (void *)&test_case_channel_set_poll_cb_03_state)
  };
  total_tests += sizeof(blackbox_channel_set_poll_cb_tests) / sizeof(blackbox_channel_set_poll_cb_tests[0]);

  return cmocka_run_group_tests(blackbox_channel_set_poll_cb_tests ,NULL , NULL);
}
