/*
    test_cases_channel_get_flags.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_get_flags.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
/* Modify this to change the port number */
#define PORT 8000

static void test_case_channel_get_flags_01(void **state);
static bool test_steps_channel_get_flags_01(void);
static void test_case_channel_get_flags_02(void **state);
static bool test_steps_channel_get_flags_02(void);
static void test_case_channel_get_flags_03(void **state);
static bool test_steps_channel_get_flags_03(void);

static black_box_state_t test_case_channel_get_flags_01_state = {
    .test_case_name = "test_case_channel_get_flags_01",
};
static black_box_state_t test_case_channel_get_flags_02_state = {
    .test_case_name = "test_case_channel_get_flags_02",
};
static black_box_state_t test_case_channel_get_flags_03_state = {
    .test_case_name = "test_case_channel_get_flags_03",
};
static black_box_state_t test_case_channel_get_flags_04_state = {
    .test_case_name = "test_case_channel_get_flags_04",
};


/* Execute meshlink_channel_get_flags Test Case # 1 - Valid case*/
static void test_case_channel_get_flags_01(void **state) {
    execute_test(test_steps_channel_get_flags_01, state);
    return;
}
/* Test Steps for meshlink_channel_get_flags Test Case # 1

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself (with TCP semantic here)
    3. Get flag(s) of that channel

    Expected Result:
    API returning exact flag that has been assigned while opening (here TCP)
*/
static bool test_steps_channel_get_flags_01(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("getflagsconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, NULL);

  assert(meshlink_start(mesh_handle));

  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);
  sleep(1);

  /* Passing all valid arguments for meshlink_channel_open_ex */
  meshlink_channel_t *channel;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
  assert(channel!= NULL);

  // Obtaining channel flags using meshlink_channel_get_flags
  uint32_t flags = meshlink_channel_get_flags(mesh_handle, channel);
  assert_int_equal(flags, MESHLINK_CHANNEL_TCP);

  meshlink_close(mesh_handle);
  meshlink_destroy("getflagsconf");

  return true;
}

/* Execute meshlink_channel_get_flags Test Case # 2 - Invalid case*/
static void test_case_channel_get_flags_02(void **state) {
    execute_test(test_steps_channel_get_flags_02, state);
    return;
}
/* Test Steps for meshlink_channel_get_flags Test Case # 2

    Test Steps:
    1. Run NUT(Node Under Test)
    2. Open channel to ourself (with TCP semantic here)
    3. Call meshlink_channel_get_flags by passing NULL as mesh handle argument

    Expected Result:
    API reporting error accordingly.
*/
static bool test_steps_channel_get_flags_02(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("getflagsconf", "nut", "node_sim", 1);
  assert(mesh_handle);

  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, NULL);

  assert(meshlink_start(mesh_handle));

  /* Getting node handle for itself */
  meshlink_node_t *node = meshlink_get_self(mesh_handle);
  assert(node != NULL);
  sleep(1);

  /* Passing all valid arguments for meshlink_channel_open_ex */
  meshlink_channel_t *channel;
  channel = meshlink_channel_open_ex(mesh_handle, node, PORT, NULL, NULL, 0, MESHLINK_CHANNEL_TCP);
  assert(channel!= NULL);

  // passing NULL as mesh handle argument for meshlink_channel_get_flags
  uint32_t flags = meshlink_channel_get_flags(NULL, channel);

  assert_int_equal((int32_t)flags, -1);
  assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

  meshlink_close(mesh_handle);
  meshlink_destroy("getflagsconf");
  return true;
}

/* Execute meshlink_channel_get flags Test Case # 3 - Invalid case*/
static void test_case_channel_get_flags_03(void **state) {
    execute_test(test_steps_channel_get_flags_03, state);
    return;
}
/* Test Steps for meshlink_channel_get_flags Test Case # 3

    Test Steps:
    1. Run NUT(Node Under Test)
    3. Call meshlink_channel_get_flags by passing NULL as channel handle argument

    Expected Result:
    API reporting error accordingly.
*/
static bool test_steps_channel_get_flags_03(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Create meshlink instance */
  meshlink_handle_t *mesh_handle = meshlink_open("getflagsconf", "nut", "node_sim", 1);
  assert(mesh_handle);
  meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
  meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);
	meshlink_set_channel_accept_cb(mesh_handle, NULL);

  assert(meshlink_start(mesh_handle));

  // passing NULL as channel handle argument for meshlink_channel_get_flags
  uint32_t flags = meshlink_channel_get_flags(mesh_handle, NULL);

  assert_int_equal((int32_t)flags, -1);
  assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

  meshlink_close(mesh_handle);
  meshlink_destroy("getflagsconf");
  return true;
}


int test_meshlink_channel_get_flags(void) {
  const struct CMUnitTest blackbox_channel_get_flags_tests[] = {
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_01, NULL, NULL,
            (void *)&test_case_channel_get_flags_01_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_02, NULL, NULL,
            (void *)&test_case_channel_get_flags_02_state),
    cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_03, NULL, NULL,
            (void *)&test_case_channel_get_flags_03_state)
  };

  total_tests += sizeof(blackbox_channel_get_flags_tests) / sizeof(blackbox_channel_get_flags_tests[0]);

  return cmocka_run_group_tests(blackbox_channel_get_flags_tests ,NULL , NULL);
}
