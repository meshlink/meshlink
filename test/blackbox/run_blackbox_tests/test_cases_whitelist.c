/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_whitelist.c -- Execution of specific meshlink black box test cases
 * @see
 * @author    Sri Harsha K, sriharsha@elear.solutions
 * @copyright 2017  Guus Sliepen <guus@meshlink.io>
 *                  Manav Kumar Mehta <manavkumarm@yahoo.com>
 * @license   To any person (the "Recipient") obtaining a copy of this software and
 *            associated documentation files (the "Software"):\n
 *            All information contained in or disclosed by this software is
 *            confidential and proprietary information of Elear Solutions Tech
 *            Private Limited and all rights therein are expressly reserved.
 *            By accepting this material the recipient agrees that this material and
 *            the information contained therein is held in confidence and in trust
 *            and will NOT be used, copied, modified, merged, published, distributed,
 *            sublicensed, reproduced in whole or in part, nor its contents revealed
 *            in any manner to others without the express written permission of
 *            Elear Solutions Tech Private Limited.
 */
/*************************************************************************************/
/*===================================================================================*/
#include "execute_tests.h"
#include "test_cases_whitelist.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include <string.h>

/*************************************************************************************
 *                          LOCAL MACROS                                             *
 *************************************************************************************/

/*************************************************************************************
 *                          LOCAL PROTOTYPES                                         *
 *************************************************************************************/
static void test_case_mesh_whitelist_01(void **state);
static bool test_steps_mesh_whitelist_01(void);
static void test_case_mesh_whitelist_02(void **state);
static bool test_steps_mesh_whitelist_02(void);
static void test_case_mesh_whitelist_03(void **state);
static bool test_steps_mesh_whitelist_03(void);

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* State structure for meshlink_whitelist Test Case #1 */
static black_box_state_t test_mesh_whitelist_01_state = {
    /* test_case_name = */ "test_case_mesh_whitelist_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_whitelist Test Case #2 */
static black_box_state_t test_mesh_whitelist_02_state = {
    /* test_case_name = */ "test_case_mesh_whitelist_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_whitelist Test Case #3 */
static black_box_state_t test_mesh_whitelist_03_state = {
    /* test_case_name = */ "test_case_mesh_whitelist_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* Execute meshlink_whitelist Test Case # 1*/
static void test_case_mesh_whitelist_01(void **state) {
	 execute_test(test_steps_mesh_whitelist_01, state);
   return;
}

static void receive(meshlink_handle_t *mesh, meshlink_node_t *src, const void *data, size_t len) {
	const char *msg = data;

	if(!len) {
		fprintf(stderr, "Received invalid data from %s\n", src->name);
		return;
	}

	fprintf(stderr, "%s says: %s\n", src->name, msg);
}

static volatile bool node_reachable = false;

static void status_cb4(meshlink_handle_t *mesh, meshlink_node_t *node, bool reachable) {
	if(!strcmp(node->name, "bar"))
		node_reachable = reachable;
}


/* Test Steps for meshlink_whitelist Test Case # 1*/
static bool test_steps_mesh_whitelist_01(void) {
	bool result = false;
	char *msg = NULL;
	char buf[] = "bar";
	msg = buf;
	char *mes = NULL;
	char buffer[] = "foo";
	mes = buffer;	
	size_t len = sizeof(buf);
	size_t leng = sizeof(buffer);
	// Open two new meshlink instance.
	meshlink_destroy("whitelist_conf.1");
	meshlink_destroy("whitelist_conf.2");
	meshlink_handle_t *mesh1 = meshlink_open("whitelist_conf.1", "foo", "blacklist", DEV_CLASS_BACKBONE);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("whitelist_conf.2", "bar", "blacklist", DEV_CLASS_BACKBONE);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_set_receive_cb(mesh2, receive);
	meshlink_set_receive_cb(mesh1, receive);	

	// Disable local discovery

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");
	meshlink_add_address(mesh2, "localhost");

	char *data = meshlink_export(mesh1);
	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(data);

	data = meshlink_export(mesh2);
	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}


	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(data);

	// Start both instances

	meshlink_set_node_status_cb(mesh1, status_cb4);

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}

	// Wait for the two to connect.


	sleep(2);
	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	if(!bar) {
		fprintf(stderr, "Bar did not know about node bar\n");
		return false;
	}
	meshlink_node_t *foo = meshlink_get_node(mesh2, "foo");
	if(!foo) {
		fprintf(stderr, "Bar did not know about node bar\n");
		return false;
	}

	result = meshlink_send(mesh1, bar, msg, len);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status6: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", msg, bar->name);
		result = true;
	}
	
	meshlink_blacklist(mesh1, foo);
	meshlink_whitelist(mesh1, foo);
	
	sleep(2);
	result = meshlink_send(mesh2, foo, mes, leng);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status6: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", mes, foo->name);
		result = true;
	}
	
	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("whitelist_conf.1");
	meshlink_destroy("whitelist_conf.2");
	return result;
}

/* Execute meshlink_whitelist Test Case # 2*/
static void test_case_mesh_whitelist_02(void **state) {
	 execute_test(test_steps_mesh_whitelist_02, state);
   return;
}

/* Test Steps for meshlink_whitelist Test Case # 2*/
static bool test_steps_mesh_whitelist_02(void) {
	bool result = false;
	char *msg = NULL;
	char buf[] = "bar";
	msg = buf;
	char *mes = NULL;
	char buffer[] = "foo";
	mes = buffer;	
	size_t len = sizeof(buf);
	size_t leng = sizeof(buffer);
	// Open two new meshlink instance.
	meshlink_destroy("whitelist_conf.3");
	meshlink_destroy("whitelist_conf.4");
	meshlink_handle_t *mesh1 = meshlink_open("whitelist_conf.3", "foo", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("whitelist_conf.4", "bar", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_set_receive_cb(mesh2, receive);
	meshlink_set_receive_cb(mesh1, receive);	

	// Disable local discovery

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");
	meshlink_add_address(mesh2, "localhost");

	char *data = meshlink_export(mesh1);
	assert(data != NULL);
	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(data);

	data = meshlink_export(mesh2);
	assert(data != NULL);
	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}


	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(data);

	// Start both instances

	meshlink_set_node_status_cb(mesh1, status_cb4);

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}

	// Wait for the two to connect.


	sleep(2);
	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar != NULL);
	if(!bar) {
		fprintf(stderr, "Bar did not know about node bar\n");
		return false;
	}
	meshlink_node_t *foo = meshlink_get_node(mesh2, "foo");
	assert(foo != NULL);
	if(!foo) {
		fprintf(stderr, "Bar did not know about node bar\n");
		return false;
	}

	result = meshlink_send(mesh1, bar, msg, len);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status6: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", msg, bar->name);
		result = true;
	}
	
	meshlink_blacklist(mesh1, foo);
	meshlink_whitelist(NULL, foo);
	
	sleep(2);
	result = meshlink_send(mesh2, foo, mes, leng);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status6: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", mes, foo->name);
		result = true;
	}
	
	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("whitelist_conf.3");
	meshlink_destroy("whitelist_conf.4");
	return result;
}

/* Execute meshlink_whitelist Test Case # 3*/
static void test_case_mesh_whitelist_03(void **state) {
	 execute_test(test_steps_mesh_whitelist_03, state);
   return;
}

/* Test Steps for meshlink_whitelist Test Case # 3*/
static bool test_steps_mesh_whitelist_03(void) {
	bool result = false;
	char *msg = NULL;
	char buf[] = "bar";
	msg = buf;
	char *mes = NULL;
	char buffer[] = "foo";
	mes = buffer;	
	size_t len = sizeof(buf);
	size_t leng = sizeof(buffer);
	// Open two new meshlink instance.
	meshlink_destroy("whitelist_conf.5");
	meshlink_destroy("whitelist_conf.6");
	meshlink_handle_t *mesh1 = meshlink_open("whitelist_conf.5", "foo", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("whitelist_conf.6", "bar", "blacklist", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_set_receive_cb(mesh2, receive);
	meshlink_set_receive_cb(mesh1, receive);	

	// Disable local discovery

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");
	meshlink_add_address(mesh2, "localhost");

	char *data = meshlink_export(mesh1);
	assert(data != NULL);
	if(!data) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, data)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(data);

	data = meshlink_export(mesh2);
	assert(data != NULL);
	if(!data) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}


	if(!meshlink_import(mesh1, data)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(data);

	// Start both instances

	meshlink_set_node_status_cb(mesh1, status_cb4);

	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}

	// Wait for the two to connect.


	sleep(2);
	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
	assert(bar != NULL);
	if(!bar) {
		fprintf(stderr, "Bar did not know about node bar\n");
		return false;
	}
	meshlink_node_t *foo = meshlink_get_node(mesh2, "foo");
	assert(foo != NULL);
	if(!foo) {
		fprintf(stderr, "Bar did not know about node bar\n");
		return false;
	}

	result = meshlink_send(mesh1, bar, msg, len);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status6: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", msg, bar->name);
		result = true;
	}
	
	meshlink_blacklist(mesh1, foo);
	meshlink_whitelist(mesh1, NULL);
	
	sleep(2);
	result = meshlink_send(mesh2, foo, mes, leng);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status6: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", mes, foo->name);
		result = true;
	}
	
	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("whitelist_conf.5");
	meshlink_destroy("whitelist_conf.6");
	return result;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
int test_meshlink_whitelist(void) {
		const struct CMUnitTest blackbox_whitelist_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_whitelist_01, NULL, NULL,
            (void *)&test_mesh_whitelist_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_whitelist_02, NULL, NULL,
            (void *)&test_mesh_whitelist_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_whitelist_03, NULL, NULL,
            (void *)&test_mesh_whitelist_03_state)
		};

  total_tests += sizeof(blackbox_whitelist_tests) / sizeof(blackbox_whitelist_tests[0]);

  return cmocka_run_group_tests(blackbox_whitelist_tests, NULL, NULL);
}
