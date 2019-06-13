/*
    test_cases_channel_close.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>

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
#include "test_cases_channel_close.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

static void test_case_mesh_channel_close_01(void **state);
static bool test_steps_mesh_channel_close_01(void);
static void test_case_mesh_channel_close_02(void **state);
static bool test_steps_mesh_channel_close_02(void);

/* State structure for meshlink_channel_close Test Case #1 */
static black_box_state_t test_mesh_channel_close_01_state = {
	.test_case_name = "test_case_mesh_channel_close_01",
};

/* State structure for meshlink_channel_close Test Case #2 */
static black_box_state_t test_mesh_channel_close_02_state = {
	.test_case_name = "test_case_mesh_channel_close_02",
};

/* Execute meshlink_channel_close Test Case # 1*/
static void test_case_mesh_channel_close_01(void **state) {
	execute_test(test_steps_mesh_channel_close_01, state);
	return;
}

/* Test Steps for meshlink_channel_close Test Case # 1*/
static bool test_steps_mesh_channel_close_01(void) {
	meshlink_destroy("chan_close_conf.3");
	meshlink_destroy("chan_close_conf.4");

	// Open two new meshlink instance.
	meshlink_handle_t *mesh1 = meshlink_open("chan_close_conf.3", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);

	meshlink_handle_t *mesh2 = meshlink_open("chan_close_conf.4", "bar", "channels", DEV_CLASS_BACKBONE);
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
	sleep(2);

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar != NULL);
	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, NULL, NULL, 0);
	assert(channel != NULL);

	meshlink_channel_close(NULL, channel);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);


	// Clean up.

	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_close_conf.3");
	meshlink_destroy("chan_close_conf.4");
	return true;
}

/* Execute meshlink_channel_close Test Case # 2*/
static void test_case_mesh_channel_close_02(void **state) {
	execute_test(test_steps_mesh_channel_close_02, state);
	return;
}

/* Test Steps for meshlink_channel_close Test Case # 2*/
static bool test_steps_mesh_channel_close_02(void) {
	meshlink_destroy("chan_close_conf.5");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh = meshlink_open("chan_close_conf.5", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh != NULL);

	// Start both instances
	assert(meshlink_start(mesh));

	// Pass NULL as mesh handle
	meshlink_channel_close(mesh, NULL);
	assert_int_equal(meshlink_errno, MESHLINK_EINVAL);

	// Clean up.

	meshlink_close(mesh);
	meshlink_destroy("chan_close_conf.5");
	return true;
}

int test_meshlink_channel_close(void) {
	const struct CMUnitTest blackbox_channel_close_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_close_01, NULL, NULL,
		                (void *)&test_mesh_channel_close_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_close_02, NULL, NULL,
		                (void *)&test_mesh_channel_close_02_state)
	};

	total_tests += sizeof(blackbox_channel_close_tests) / sizeof(blackbox_channel_close_tests[0]);

	return cmocka_run_group_tests(blackbox_channel_close_tests, NULL, NULL);
}
