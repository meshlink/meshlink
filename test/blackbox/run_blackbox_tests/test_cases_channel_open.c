/*
    test_cases_channel_open.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_channel_open.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_channel_open_01(void **state);
static bool test_steps_mesh_channel_open_01(void);
static void test_case_mesh_channel_open_02(void **state);
static bool test_steps_mesh_channel_open_02(void);
static void test_case_mesh_channel_open_03(void **state);
static bool test_steps_mesh_channel_open_03(void);
static void test_case_mesh_channel_open_04(void **state);
static bool test_steps_mesh_channel_open_04(void);

/* State structure for meshlink_channel_open Test Case #1 */
static black_box_state_t test_mesh_channel_open_01_state = {
    .test_case_name = "test_case_mesh_channel_open_01",
};

/* State structure for meshlink_channel_open Test Case #2 */
static black_box_state_t test_mesh_channel_open_02_state = {
    .test_case_name = "test_case_mesh_channel_open_02",
};

/* State structure for meshlink_channel_open Test Case #3 */
static black_box_state_t test_mesh_channel_open_03_state = {
    .test_case_name = "test_case_mesh_channel_open_03",
};

/* State structure for meshlink_channel_open Test Case #4 */
static black_box_state_t test_mesh_channel_open_04_state = {
    .test_case_name = "test_case_mesh_channel_open_04",
};

/* Execute meshlink_channel_open Test Case # 1*/
static void test_case_mesh_channel_open_01(void **state) {
	 execute_test(test_steps_mesh_channel_open_01, state);
   return;
}

static void receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;
}

/* Test Steps for meshlink_channel_open Test Case # 1

    Test Steps:
    1. Open both the node instances
    2. Join bar node with foo
    3. Open channel between the nodes

    Expected Result:
    meshlink_channel_open should open a channel by returning a channel handler
*/
static bool test_steps_mesh_channel_open_01(void) {
	meshlink_destroy("channels_conf.1");
	meshlink_destroy("channels_conf.2");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh1 = meshlink_open("channels_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	meshlink_handle_t *mesh2 = meshlink_open("channels_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);

	// Import and export both side's data
	char *exp = meshlink_export(mesh1);
	assert(exp != NULL);
	assert(meshlink_import(mesh2, exp));
	free(exp);
	exp = meshlink_export(mesh2);
	assert(exp != NULL);
	assert(meshlink_import(mesh1, exp));
	free(exp);

	// Start both instances
	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));
  sleep(2);

	// Open a channel from foo to bar.
	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
  assert(bar != NULL);
	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7000, receive_cb, NULL, 0);
	assert_int_not_equal(channel, NULL);

	// Clean up.
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("channels_conf.1");
	meshlink_destroy("channels_conf.2");
	return true;
}

/* Execute meshlink_channel_open Test Case # 2

    Test Steps:
    1. Open both the node instances
    2. Join bar node with foo
    3. Open channel between the nodes with NULL as receive callback argument

    Expected Result:
    meshlink_channel_open should open a channel by returning a channel handler
*/
static void test_case_mesh_channel_open_02(void **state) {
	 execute_test(test_steps_mesh_channel_open_02, state);
   return;
}

/* Test Steps for meshlink_channel_open Test Case # 2*/
static bool test_steps_mesh_channel_open_02(void) {
	meshlink_destroy("channels_conf.3");
	meshlink_destroy("channels_conf.4");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh1 = meshlink_open("channels_conf.3", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("channels_conf.4", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	char *exp = meshlink_export(mesh1);
	assert(exp != NULL);
	assert(meshlink_import(mesh2, exp));
	free(exp);
	exp = meshlink_export(mesh2);
	assert(exp != NULL);
	assert(meshlink_import(mesh1, exp));
	free(exp);

	// Start both instances
	assert(meshlink_start(mesh1));
	assert(meshlink_start(mesh2));
  sleep(1);

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
  assert(bar != NULL);

	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7000, NULL, NULL, 0);
	assert_int_not_equal(channel, NULL);

	// Clean up.
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("channels_conf.3");
	meshlink_destroy("channels_conf.4");
	return true;
}

/* Execute meshlink_channel_open Test Case # 3 */
static void test_case_mesh_channel_open_03(void **state) {
	 execute_test(test_steps_mesh_channel_open_03, state);
   return;
}

/* Test Steps for meshlink_channel_open Test Case # 3

    Test Steps:
    1. Create the node instance & obtain node handle
    2. Open a channel with NULL as mesh handle argument
        and rest other arguments being valid.

    Expected Result:
    meshlink_channel_open API handles the invalid parameter
    when called by giving proper error number.
*/
static bool test_steps_mesh_channel_open_03(void) {
	meshlink_destroy("channels_conf.5");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh = meshlink_open("channels_conf.5", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh != NULL);

	meshlink_node_t *node = meshlink_get_self(mesh);
	assert(node);

	meshlink_channel_t *channel = meshlink_channel_open(NULL, node, 7000, NULL, NULL, 0);
	assert_int_equal(channel, NULL);

	// Clean up.
	meshlink_close(mesh);
	meshlink_destroy("channels_conf.5");
	return true;
}

/* Execute meshlink_channel_open Test Case # 4*/
static void test_case_mesh_channel_open_04(void **state) {
	 execute_test(test_steps_mesh_channel_open_04, state);
   return;
}

/* Test Steps for meshlink_channel_open Test Case # 4

    Test Steps:
    1. Create the node instance & obtain node handle
    2. Open a channel with NULL as node handle argument
        and rest other arguments being valid.

    Expected Result:
    meshlink_channel_open API handles the invalid parameter
    when called by giving proper error number.
*/
static bool test_steps_mesh_channel_open_04(void) {
	meshlink_destroy("channels_conf.7");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh = meshlink_open("channels_conf.7", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh != NULL);

	// Start both instances
	assert(meshlink_start(mesh));

	meshlink_channel_t *channel = meshlink_channel_open(mesh, NULL, 7000, NULL, NULL, 0);
	assert_int_equal(channel, NULL);

	// Clean up.
	meshlink_close(mesh);
	meshlink_destroy("channels_conf.7");
	return true;
}

int test_meshlink_channel_open(void) {
  const struct CMUnitTest blackbox_channel_open_tests[] = {
      cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_open_01, NULL, NULL,
          (void *)&test_mesh_channel_open_01_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_open_02, NULL, NULL,
          (void *)&test_mesh_channel_open_02_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_open_03, NULL, NULL,
          (void *)&test_mesh_channel_open_03_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_open_04, NULL, NULL,
          (void *)&test_mesh_channel_open_04_state)
  };

  total_tests += sizeof(blackbox_channel_open_tests) / sizeof(blackbox_channel_open_tests[0]);

  return cmocka_run_group_tests(blackbox_channel_open_tests, NULL, NULL);
}
