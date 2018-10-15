/*
    test_cases_stop_close.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_stop_close.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_close_01(void **state);
static bool test_steps_mesh_close_01(void);
static void test_case_mesh_stop_01(void **state);
static bool test_steps_mesh_stop_01(void);

/* State structure for meshlink_close Test Case #1 */
static black_box_state_t test_mesh_close_01_state = {
    .test_case_name = "test_case_mesh_close_01",
};

/* State structure for meshlink_close Test Case #1 */
static black_box_state_t test_mesh_stop_01_state = {
    .test_case_name = "test_case_mesh_stop_01",
};

/* Execute meshlink_close Test Case # 1*/
static void test_case_mesh_close_01(void **state) {
	 execute_test(test_steps_mesh_close_01, state);
   return;
}

static bool packet_send = false;

static void receive_cb(meshlink_handle_t *mesh, meshlink_node_t *source, const void *data, size_t len) {
	(void)mesh;
	(void)source;

	fprintf(stderr, "RECEIVED SOMETHING\n");
	printf("Hey got something\n");
	packet_send = true;
	return;
}

static void log_cb(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)mesh;

	fprintf(stderr, "[%d] %s\n", level, text);
}

/* Test Steps for meshlink_close Test Case # 1*/

static bool test_steps_mesh_close_01(void) {
  // Create an instance

	/*bool result = false;
	meshlink_handle_t *mesh = meshlink_open("stop_conf.1", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	meshlink_set_receive_cb(mesh, receive_cb);
	meshlink_set_log_cb(mesh, MESHLINK_DEBUG, log_cb);

  // Start the node
sleep(2);
	result = meshlink_start(mesh);
	assert(result);


	// Send a packet to it self

	meshlink_node_t *self = meshlink_get_self(mesh);
	assert(self);
	sleep(2);
	packet_send = false;
  assert(meshlink_send(mesh, self, "test", 5));
  assert(packet_send);

	meshlink_close(mesh);*/


		bool result = false;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	meshlink_handle_t *mesh = meshlink_open("send_conf.1", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh);

	meshlink_set_receive_cb(mesh, receive_cb);

	if (!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	meshlink_node_t *dest_node = meshlink_get_self(mesh);
	if(!dest_node) {
		fprintf(stderr, "meshlink_get_self status1: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	assert(dest_node != NULL);
	result = meshlink_send(mesh, dest_node, data, len);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status1: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = true;
	}

	meshlink_close(mesh);


	packet_send = false;
	result = meshlink_send(mesh, dest_node, "test", 5);

	meshlink_destroy("stop_conf.1");
  return !packet_send && result;
}

/* Execute meshlink_stop Test Case # 1*/
static void test_case_mesh_stop_01(void **state) {
	 execute_test(test_steps_mesh_stop_01, state);
   return;
}

/* Test Steps for meshlink_stop Test Case # 1*/
static bool test_steps_mesh_stop_01(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("stop_conf.2", "foo", "chat", DEV_CLASS_STATIONARY);
		assert(mesh != NULL);
		result = meshlink_start(mesh);
		assert(result);
		meshlink_stop(mesh);
		meshlink_destroy("stop_conf.2");
    return result;
}

int test_meshlink_stop_close(void) {
		const struct CMUnitTest blackbox_stop_close_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_close_01, NULL, NULL,
            (void *)&test_mesh_close_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_stop_01, NULL, NULL,
            (void *)&test_mesh_stop_01_state)
		};

  total_tests += sizeof(blackbox_stop_close_tests) / sizeof(blackbox_stop_close_tests[0]);

  return cmocka_run_group_tests(blackbox_stop_close_tests, NULL, NULL);
}

