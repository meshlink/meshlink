/*
    test_cases_send.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_send.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_send_01(void **state);
static bool test_steps_mesh_send_01(void);
static void test_case_mesh_send_02(void **state);
static bool test_steps_mesh_send_02(void);
static void test_case_mesh_send_03(void **state);
static bool test_steps_mesh_send_03(void);

/* State structure for meshlink_send Test Case #1 */
static black_box_state_t test_mesh_send_01_state = {
    .test_case_name = "test_case_mesh_send_01",
};

/* State structure for meshlink_send Test Case #2 */
static black_box_state_t test_mesh_send_02_state = {
    .test_case_name = "test_case_mesh_send_02",
};

/* State structure for meshlink_send Test Case #3 */
static black_box_state_t test_mesh_send_03_state = {
    .test_case_name = "test_case_mesh_send_03",
};

/* State structure for meshlink_send Test Case #4 */
static black_box_state_t test_mesh_send_04_state = {
    .test_case_name = "test_case_mesh_send_04",
};

/* State structure for meshlink_send Test Case #5 */
static black_box_state_t test_mesh_send_05_state = {
    .test_case_name = "test_case_mesh_send_05",
};

/* State structure for meshlink_send Test Case #6 */
static black_box_state_t test_mesh_send_06_state = {
    .test_case_name = "test_case_mesh_send_06",
};

/* Execute meshlink_send Test Case # 1 */
static void test_case_mesh_send_01(void **state) {
    execute_test(test_steps_mesh_send_01, state);
    return;
}

static bool receive_data = false;
static void receive(meshlink_handle_t *mesh, meshlink_node_t *dest_node, const void *data, size_t len) {
	const char *msg = data;

	assert(len);

	if(!memcmp(data, "test", 5)) {
    receive_data = true;
	}
	return;
}

/* Test Steps for meshlink_send Test Case # 1

    Test Steps:
    1. Open instance of foo node
    2. Run and send data to itself

    Expected Result:
    Node should receive data sent to itself
*/
static bool test_steps_mesh_send_01(void) {
	bool result = false;

	meshlink_handle_t *mesh = meshlink_open("send_conf", "foo", "test", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	meshlink_set_receive_cb(mesh, receive);
	assert(meshlink_start(mesh));
  sleep(1);
	meshlink_node_t *dest_node = meshlink_get_self(mesh);
	assert(dest_node);

	receive_data = false;
	result = meshlink_send(mesh, dest_node, "test", 5);
	assert_int_equal(result, true);
  sleep(1);
	assert_int_equal(receive_data, true);

	meshlink_close(mesh);
	meshlink_destroy("send_conf");
  return result;
}

/* Execute meshlink_send Test Case # 2

    Test Steps:
    1. Open instance of foo node
    2. meshlink_send with NULL as mesh handle

    Expected Result:
    meshlink_send returns false because of NULL handle
*/
static void test_case_mesh_send_02(void **state) {
    execute_test(test_steps_mesh_send_02, state);
    return;
}

/* Test Steps for meshlink_send Test Case # 2*/
static bool test_steps_mesh_send_02(void) {
	meshlink_handle_t *mesh = meshlink_open("send_conf", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	meshlink_set_receive_cb(mesh, receive);

	assert(meshlink_start(mesh));
	meshlink_node_t *dest_node = meshlink_get_self(mesh);
	assert(dest_node);

	bool ret = meshlink_send(NULL, dest_node, "test", 5);
  assert_int_equal(ret, false);

	meshlink_close(mesh);
	meshlink_destroy("send_conf");
  return true;
}

/* Execute meshlink_send Test Case # 3

    Test Steps:
    1. Open instance of foo node
    2. meshlink_send with NULL as node handle

    Expected Result:
    meshlink_send returns false because of NULL handle
*/
static void test_case_mesh_send_03(void **state) {
    execute_test(test_steps_mesh_send_03, state);
    return;
}

/* Test Steps for meshlink_send Test Case # 3*/
static bool test_steps_mesh_send_03(void) {
	meshlink_handle_t *mesh = meshlink_open("send_conf", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	meshlink_set_receive_cb(mesh, receive);

	assert(meshlink_start(mesh));

	bool ret = meshlink_send(mesh, NULL, "test", 5);
  assert_int_equal(ret, false);

	meshlink_close(mesh);
	meshlink_destroy("send_conf");
  return true;
}

int test_meshlink_send(void) {
  const struct CMUnitTest blackbox_send_tests[] = {
      cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_01, NULL, NULL,
          (void *)&test_mesh_send_01_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_02, NULL, NULL,
          (void *)&test_mesh_send_02_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_03, NULL, NULL,
          (void *)&test_mesh_send_03_state)
  };

  total_tests += sizeof(blackbox_send_tests) / sizeof(blackbox_send_tests[0]);

  return cmocka_run_group_tests(blackbox_send_tests, NULL, NULL);
}
