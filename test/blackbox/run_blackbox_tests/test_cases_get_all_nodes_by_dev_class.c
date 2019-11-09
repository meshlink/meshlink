/*
    test_cases_get_all_nodes_by_dev_class.c.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2019  Guus Sliepen <guus@meshlink.io>

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

#ifdef NDEBUG
#undef NDEBUG
#endif

#include "execute_tests.h"
#include "test_cases_get_all_nodes_by_dev_class.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

static void test_case_mesh_get_node_by_dev_class_01(void **state);
static bool test_steps_mesh_get_node_by_dev_class_01(void);
static void test_case_mesh_get_node_by_dev_class_02(void **state);
static bool test_steps_mesh_get_node_by_dev_class_02(void);
static void test_case_mesh_get_node_dev_class_01(void **state);
static bool test_steps_mesh_get_node_dev_class_01(void);
static void test_case_mesh_get_node_dev_class_02(void **state);
static bool test_steps_mesh_get_node_dev_class_02(void);

/* State structure for meshlink_get_node Test Case #1 */
static black_box_state_t test_mesh_get_node_by_dev_class_01_state = {
	.test_case_name = "test_case_mesh_get_node_by_dev_class_01",
};

/* State structure for meshlink_get_node Test Case #2 */
static black_box_state_t test_mesh_get_node_by_dev_class_02_state = {
	.test_case_name = "test_case_mesh_get_node_by_dev_class_02",
};

/* State structure for meshlink_get_node Test Case #3 */
static black_box_state_t test_mesh_get_node_01_state = {
	.test_case_name = "test_mesh_get_node_01_state",
};

/* State structure for meshlink_get_node Test Case #4 */
static black_box_state_t test_mesh_get_node_02_state = {
	.test_case_name = "test_mesh_get_node_02_state",
};

static void log_message(meshlink_handle_t *mesh, meshlink_log_level_t level, const char *text) {
	(void)mesh;

	static const char *levelstr[] = {
		[MESHLINK_DEBUG] = "\x1b[34mDEBUG",
		[MESHLINK_INFO] = "\x1b[32mINFO",
		[MESHLINK_WARNING] = "\x1b[33mWARNING",
		[MESHLINK_ERROR] = "\x1b[31mERROR",
		[MESHLINK_CRITICAL] = "\x1b[31mCRITICAL",
	};

	fprintf(stderr, "%s(%s):\x1b[0m %s\n", mesh->name, levelstr[level], text);
}

/* Execute meshlink_get_node Test Case # 1 */
static void test_case_mesh_get_node_by_dev_class_01(void **state) {
	execute_test(test_steps_mesh_get_node_by_dev_class_01, state);
}

/* Test Steps for meshlink_get_node Test Case # 1

    Test Steps:
    1. Open nut, peer1, relay1, relay2, relay3 node instances, export and
        import the configuration of NUT with other nodes.
    2. Run the node instances.
    3. Call meshlink_get_all_nodes_by_dev_class API with NULL as nodes array parameter
        for DEV_CLASS_STATIONARY
    4. Call meshlink_get_all_nodes_by_dev_class API with previously allocated nodes array
        parameter for DEV_CLASS_BACKBONE
    5. Call meshlink_get_all_nodes_by_dev_class API with previously allocated nodes array
        parameter for DEV_CLASS_PORTABLE

    Expected Result:
    meshlink_get_all_nodes_by_dev_class API should return appropriate node array pointer and
    node member parameter when called and return accordingly.
*/
static bool test_steps_mesh_get_node_by_dev_class_01(void) {
	meshlink_node_t **nodes;
	size_t nnodes = 0, i;

	/* Create meshlink instance for NUT */
	meshlink_handle_t *mesh_nut = meshlink_open("getnodeconf.1", "nut", "node_sim", DEV_CLASS_STATIONARY);
	assert(mesh_nut);
	meshlink_set_log_cb(mesh_nut, TEST_MESHLINK_LOG_LEVEL, log_message);

	/* Create meshlink instance for peer1 */
	meshlink_handle_t *mesh_peer1 = meshlink_open("getnodeconf.2", "peer1", "node_sim", DEV_CLASS_STATIONARY);
	assert(mesh_peer1);
	meshlink_set_log_cb(mesh_peer1, TEST_MESHLINK_LOG_LEVEL, log_message);

	/* Create meshlink instance for relay1 */
	meshlink_handle_t *mesh_relay1 = meshlink_open("getnodeconf.3", "relay1", "node_sim", DEV_CLASS_BACKBONE);
	assert(mesh_relay1);
	meshlink_set_log_cb(mesh_relay1, TEST_MESHLINK_LOG_LEVEL, log_message);

	/* Create meshlink instance for relay2 */
	meshlink_handle_t *mesh_relay2 = meshlink_open("getnodeconf.4", "relay2", "node_sim", DEV_CLASS_BACKBONE);
	assert(mesh_relay2);
	meshlink_set_log_cb(mesh_relay2, TEST_MESHLINK_LOG_LEVEL, log_message);

	/* Create meshlink instance for relay3 */
	meshlink_handle_t *mesh_relay3 = meshlink_open("getnodeconf.5", "relay3", "node_sim", DEV_CLASS_BACKBONE);
	assert(mesh_relay3);
	meshlink_set_log_cb(mesh_relay3, TEST_MESHLINK_LOG_LEVEL, log_message);

	/* importing and exporting mesh meta data */
	char *exp_nut = meshlink_export(mesh_nut);
	assert(exp_nut != NULL);
	char *export = meshlink_export(mesh_peer1);
	assert(export != NULL);
	assert(meshlink_import(mesh_nut, export));
	assert(meshlink_import(mesh_peer1, exp_nut));
	free(export);

	export = meshlink_export(mesh_relay1);
	assert(export != NULL);
	assert(meshlink_import(mesh_nut, export));
	assert(meshlink_import(mesh_relay1, exp_nut));
	free(export);

	export = meshlink_export(mesh_relay2);
	assert(export != NULL);
	assert(meshlink_import(mesh_nut, export));
	assert(meshlink_import(mesh_relay2, exp_nut));
	free(export);

	export = meshlink_export(mesh_relay3);
	assert(export != NULL);
	assert(meshlink_import(mesh_nut, export));
	assert(meshlink_import(mesh_relay3, exp_nut));
	free(export);
	free(exp_nut);

	nodes = meshlink_get_all_nodes_by_dev_class(mesh_nut, DEV_CLASS_STATIONARY, NULL, &nnodes);
	assert_int_not_equal(nodes, NULL);
	assert_int_equal(nnodes, 2);

	for(i = 0; i < nnodes; i++) {
		if(strcasecmp(nodes[i]->name, "nut") && strcasecmp(nodes[i]->name, "peer1")) {
			fail();
		}
	}

	nodes = meshlink_get_all_nodes_by_dev_class(mesh_nut, DEV_CLASS_BACKBONE, nodes, &nnodes);
	assert_int_not_equal(nodes, NULL);
	assert_int_equal(nnodes, 3);

	for(i = 0; i < nnodes; i++) {
		if(strcasecmp(nodes[i]->name, "relay1") && strcasecmp(nodes[i]->name, "relay2") && strcasecmp(nodes[i]->name, "relay3")) {
			fail();
		}
	}

	nodes = meshlink_get_all_nodes_by_dev_class(mesh_nut, DEV_CLASS_PORTABLE, nodes, &nnodes);
	assert_int_equal(nodes, NULL);
	assert_int_equal(nnodes, 0);
	assert_int_equal(meshlink_errno, 0);

	free(nodes);
	meshlink_close(mesh_nut);
	meshlink_close(mesh_peer1);
	meshlink_close(mesh_relay1);
	meshlink_close(mesh_relay2);
	meshlink_close(mesh_relay3);

	return true;
}

/* Execute meshlink_get_node Test Case # 2 - Invalid case
    Passing invalid parameters as input arguments */
static void test_case_mesh_get_node_by_dev_class_02(void **state) {
	execute_test(test_steps_mesh_get_node_by_dev_class_02, state);
}

/* Test Steps for meshlink_get_node Test Case # 2

    Test Steps:
    1. Create NUT
    2. Call meshlink_get_all_nodes_by_dev_class API with invalid parameters

    Expected Result:
    meshlink_get_all_nodes_by_dev_class API should return NULL and set appropriate
    meshlink_errno.
*/
static bool test_steps_mesh_get_node_by_dev_class_02(void) {
	meshlink_node_t **nodes;
	size_t nnodes = 0;

	assert(meshlink_destroy("getnodeconf.1"));

	/* Create meshlink instance for NUT */
	meshlink_handle_t *mesh_nut = meshlink_open("getnodeconf.1", "nut", "node_sim", DEV_CLASS_STATIONARY);
	assert(mesh_nut);
	meshlink_set_log_cb(mesh_nut, TEST_MESHLINK_LOG_LEVEL, log_message);

	nodes = meshlink_get_all_nodes_by_dev_class(mesh_nut, DEV_CLASS_COUNT + 10, NULL, &nnodes);
	assert_int_equal(nodes, NULL);
	assert_int_not_equal(meshlink_errno, 0);

	nodes = meshlink_get_all_nodes_by_dev_class(mesh_nut, DEV_CLASS_STATIONARY, NULL, NULL);
	assert_int_equal(nodes, NULL);
	assert_int_not_equal(meshlink_errno, 0);

	nodes = meshlink_get_all_nodes_by_dev_class(NULL, DEV_CLASS_STATIONARY, NULL, &nnodes);
	assert_int_equal(nodes, NULL);
	assert_int_not_equal(meshlink_errno, 0);

	meshlink_close(mesh_nut);
	assert(meshlink_destroy("getnodeconf.1"));
	return true;
}

/* Execute meshlink_get_node_dev_class Test Case # 1 */
static void test_case_mesh_get_node_dev_class_01(void **state) {
	execute_test(test_steps_mesh_get_node_dev_class_01, state);
}

/* Test Steps for meshlink_get_node_dev_class Test Case # 1

    Test Steps:
    1. Create NUT node with DEV_CLASS_STATIONARY device class and obtain node handle
    2. Call meshlink_get_node_dev_class API

    Expected Result:
    meshlink_get_node_dev_class API should return DEV_CLASS_STATIONARY device class
*/
static bool test_steps_mesh_get_node_dev_class_01(void) {
	assert(meshlink_destroy("getnodeconf.1"));

	/* Create meshlink instance for NUT */
	meshlink_handle_t *mesh_nut = meshlink_open("getnodeconf.1", "nut", "node_sim", DEV_CLASS_STATIONARY);
	assert(mesh_nut);
	meshlink_set_log_cb(mesh_nut, TEST_MESHLINK_LOG_LEVEL, log_message);

	meshlink_node_t *node;
	node = meshlink_get_self(mesh_nut);
	assert(node);

	dev_class_t dev_class = meshlink_get_node_dev_class(mesh_nut, node);
	assert_int_equal(dev_class, DEV_CLASS_STATIONARY);

	meshlink_close(mesh_nut);
	assert(meshlink_destroy("getnodeconf.1"));
	return true;
}

/* Execute meshlink_get_node_dev_class Test Case # 2 */
static void test_case_mesh_get_node_dev_class_02(void **state) {
	execute_test(test_steps_mesh_get_node_dev_class_02, state);
}

/* Test Steps for meshlink_get_node_dev_class Test Case # 2

    Test Steps:
    1. Create NUT and obtain NUT node handle
    2. Call meshlink_get_node_dev_class API with invalid parameters

    Expected Result:
    meshlink_get_node_dev_class API should return NULL and set appropriate
    meshlink_errno.
*/
static bool test_steps_mesh_get_node_dev_class_02(void) {
	assert(meshlink_destroy("getnodeconf.1"));

	/* Create meshlink instance for NUT */
	meshlink_handle_t *mesh_nut = meshlink_open("getnodeconf.1", "nut", "node_sim", DEV_CLASS_STATIONARY);
	assert(mesh_nut);
	meshlink_set_log_cb(mesh_nut, TEST_MESHLINK_LOG_LEVEL, log_message);

	meshlink_node_t *node;
	node = meshlink_get_self(mesh_nut);
	assert(node);

	int dev_class = meshlink_get_node_dev_class(NULL, node);
	assert_int_equal(dev_class, -1);
	assert_int_not_equal(meshlink_errno, 0);

	dev_class = meshlink_get_node_dev_class(mesh_nut, NULL);
	assert_int_equal(dev_class, -1);
	assert_int_not_equal(meshlink_errno, 0);

	meshlink_close(mesh_nut);
	assert(meshlink_destroy("getnodeconf.1"));
	return true;
}

static int black_box_setup_test_case(void **state) {
	(void)state;

	fprintf(stderr, "Destroying confbases\n");
	assert(meshlink_destroy("getnodeconf.1"));
	assert(meshlink_destroy("getnodeconf.2"));
	assert(meshlink_destroy("getnodeconf.3"));
	assert(meshlink_destroy("getnodeconf.4"));
	assert(meshlink_destroy("getnodeconf.5"));
	meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
	meshlink_errno = MESHLINK_OK;

	return 0;
}

int test_meshlink_get_all_node_by_device_class(void) {
	const struct CMUnitTest blackbox_get_node_tests[] = {
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_by_dev_class_01, black_box_setup_test_case, black_box_setup_test_case,
		                (void *)&test_mesh_get_node_by_dev_class_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_by_dev_class_02, NULL, NULL,
		                (void *)&test_mesh_get_node_by_dev_class_02_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_dev_class_01, NULL, NULL,
		                (void *)&test_mesh_get_node_01_state),
		cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_dev_class_02, NULL, NULL,
		                (void *)&test_mesh_get_node_02_state),
	};

	total_tests += sizeof(blackbox_get_node_tests) / sizeof(blackbox_get_node_tests[0]);

	return cmocka_run_group_tests(blackbox_get_node_tests, NULL, NULL);
}
