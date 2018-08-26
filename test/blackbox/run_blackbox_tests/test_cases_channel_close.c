/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_add_addr.c -- Execution of specific meshlink black box test cases
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

/*************************************************************************************
 *                          LOCAL MACROS                                             *
 *************************************************************************************/

/*************************************************************************************
 *                          LOCAL PROTOTYPES                                         *
 *************************************************************************************/
static void test_case_mesh_channel_close_01(void **state);
static bool test_steps_mesh_channel_close_01(void);
static void test_case_mesh_channel_close_02(void **state);
static bool test_steps_mesh_channel_close_02(void);
static void test_case_mesh_channel_close_03(void **state);
static bool test_steps_mesh_channel_close_03(void);

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* State structure for meshlink_channel_close Test Case #1 */
static black_box_state_t test_mesh_channel_close_01_state = {
    /* test_case_name = */ "test_case_mesh_channel_close_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_channel_close Test Case #2 */
static black_box_state_t test_mesh_channel_close_02_state = {
    /* test_case_name = */ "test_case_mesh_channel_close_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_channel_close Test Case #3 */
static black_box_state_t test_mesh_channel_close_03_state = {
    /* test_case_name = */ "test_case_mesh_channel_close_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* Execute meshlink_channel_close Test Case # 1*/
static void test_case_mesh_channel_close_01(void **state) {
	 execute_test(test_steps_mesh_channel_close_01, state);
   return;
}

static volatile bool bar_responded = false;

static void foo_receive_cb(meshlink_handle_t *mesh, meshlink_channel_t *channel, const void *data, size_t len) {
	(void)mesh;
	(void)channel;

	printf("foo_receive_cb %zu: ", len);
	fwrite(data, 1, len, stdout);
	printf("\n");

	if(len == 5 && !memcmp(data, "Hello", 5)) {
		bar_responded = true;
	}
}

/* Test Steps for meshlink_channel_close Test Case # 1*/
static bool test_steps_mesh_channel_close_01(void) {
	bool result = false;
	char *msg = NULL;
	char buf[] = "bar";
	msg = buf;
	size_t len = sizeof(buf);
	meshlink_destroy("chan_close_conf.1");
	meshlink_destroy("chan_close_conf.2");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_close_conf.1", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("chan_close_conf.2", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *exp = meshlink_export(mesh1);
	assert(exp != NULL);
	if(!exp) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, exp)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(exp);

	exp = meshlink_export(mesh2);
	assert(exp != NULL);
	if(!exp) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh1, exp)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(exp);

	// Start both instances
	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}
	sleep(2);

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
  assert(bar != NULL);
	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return false;
	}
	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	assert(channel != NULL);
	if(!channel) {
		fprintf(stderr, "can't open channel error: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);

	meshlink_channel_close(mesh1, channel);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_close_conf.1");
	meshlink_destroy("chan_close_conf.2");
	return true;
}

/* Execute meshlink_channel_close Test Case # 2*/
static void test_case_mesh_channel_close_02(void **state) {
	 execute_test(test_steps_mesh_channel_close_02, state);
   return;
}

/* Test Steps for meshlink_channel_close Test Case # 2*/
static bool test_steps_mesh_channel_close_02(void) {
	bool result = false;
	char *msg = NULL;
	char buf[] = "bar";
	msg = buf;
	size_t len = sizeof(buf);
	meshlink_destroy("chan_close_conf.3");
	meshlink_destroy("chan_close_conf.4");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_close_conf.3", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("chan_close_conf.4", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *exp = meshlink_export(mesh1);
	assert(exp != NULL);
	if(!exp) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, exp)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(exp);

	exp = meshlink_export(mesh2);
	assert(exp != NULL);
	if(!exp) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh1, exp)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(exp);

	// Start both instances
	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}
	sleep(2);

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
  assert(bar != NULL);
	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return false;
	}
	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	assert(channel != NULL);
	if(!channel) {
		fprintf(stderr, "can't open channel error: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);

	meshlink_channel_close(NULL, channel);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_close_conf.3");
	meshlink_destroy("chan_close_conf.4");
	return true;
}

/* Execute meshlink_channel_close Test Case # 3*/
static void test_case_mesh_channel_close_03(void **state) {
	 execute_test(test_steps_mesh_channel_close_03, state);
   return;
}

/* Test Steps for meshlink_channel_close Test Case # 3*/
static bool test_steps_mesh_channel_close_03(void) {
	bool result = false;
	char *msg = NULL;
	char buf[] = "bar";
	msg = buf;
	size_t len = sizeof(buf);
	meshlink_destroy("chan_close_conf.5");
	meshlink_destroy("chan_close_conf.6");
	// Open two new meshlink instance.

	meshlink_handle_t *mesh1 = meshlink_open("chan_close_conf.5", "foo", "channels", DEV_CLASS_BACKBONE);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "Could not initialize configuration for foo\n");
		return false;
	}

	meshlink_handle_t *mesh2 = meshlink_open("chan_close_conf.6", "bar", "channels", DEV_CLASS_BACKBONE);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "Could not initialize configuration for bar\n");
		return false;
	}

	meshlink_enable_discovery(mesh1, false);
	meshlink_enable_discovery(mesh2, false);

	// Import and export both side's data

	meshlink_add_address(mesh1, "localhost");

	char *exp = meshlink_export(mesh1);
	assert(exp != NULL);
	if(!exp) {
		fprintf(stderr, "Foo could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh2, exp)) {
		fprintf(stderr, "Bar could not import foo's configuration\n");
		return false;
	}

	free(exp);

	exp = meshlink_export(mesh2);
	assert(exp != NULL);
	if(!exp) {
		fprintf(stderr, "Bar could not export its configuration\n");
		return false;
	}

	if(!meshlink_import(mesh1, exp)) {
		fprintf(stderr, "Foo could not import bar's configuration\n");
		return false;
	}

	free(exp);

	// Start both instances
	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "Foo could not start\n");
		return false;
	}

	if(!meshlink_start(mesh2)) {
		fprintf(stderr, "Bar could not start\n");
		return false;
	}
	sleep(2);

	// Open a channel from foo to bar.

	meshlink_node_t *bar = meshlink_get_node(mesh1, "bar");
  assert(bar != NULL);
	if(!bar) {
		fprintf(stderr, "Foo could not find bar\n");
		return false;
	}
	meshlink_channel_t *channel = meshlink_channel_open(mesh1, bar, 7, foo_receive_cb, NULL, 0);
	assert(channel != NULL);
	if(!channel) {
		fprintf(stderr, "can't open channel error: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);

	meshlink_channel_close(mesh1, NULL);

	// Clean up.

	meshlink_stop(mesh2);
	meshlink_stop(mesh1);
	meshlink_close(mesh2);
	meshlink_close(mesh1);
	meshlink_destroy("chan_close_conf.5");
	meshlink_destroy("chan_close_conf.6");
	return true;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
int test_meshlink_channel_close(void) {
		const struct CMUnitTest blackbox_channel_close_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_close_01, NULL, NULL,
            (void *)&test_mesh_channel_close_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_close_02, NULL, NULL,
            (void *)&test_mesh_channel_close_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_channel_close_03, NULL, NULL,
            (void *)&test_mesh_channel_close_03_state)
		};

  total_tests += sizeof(blackbox_channel_close_tests) / sizeof(blackbox_channel_close_tests[0]);

  return cmocka_run_group_tests(blackbox_channel_close_tests, NULL, NULL);
}
