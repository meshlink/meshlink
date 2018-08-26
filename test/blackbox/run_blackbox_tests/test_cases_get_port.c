/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_get_port.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_get_port.h"
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
static void test_case_mesh_get_port_01(void **state);
static bool test_steps_mesh_get_port_01(void);
static void test_case_mesh_get_port_02(void **state);
static bool test_steps_mesh_get_port_02(void);

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* State structure for meshlink_get_port Test Case #1 */
static black_box_state_t test_mesh_get_port_01_state = {
    /* test_case_name = */ "test_case_mesh_get_port_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_port Test Case #2 */
static black_box_state_t test_mesh_get_port_02_state = {
    /* test_case_name = */ "test_case_mesh_get_port_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/

/* Execute meshlink_get_port Test Case # 1 */
static void test_case_mesh_get_port_01(void **state) {
    execute_test(test_steps_mesh_get_port_01, state);
    return;
}

/* Test Steps for meshlink_get_port Test Case # 1*/
static bool test_steps_mesh_get_port_01(void) {
	bool result = false;
	int port = 0;

  meshlink_handle_t *mesh = meshlink_open("port_conf.1", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	if(!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	
	port = meshlink_get_port(mesh);
	assert(port != -1);
	if(port == -1) {
		fprintf(stderr, "meshlink_add_external_address status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "Port number used by mesh is %d\n", port);
		result = true;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
	meshlink_destroy("port_conf.1");
  return result;
}

/* Execute meshlink_get_port Test Case # 2 */
static void test_case_mesh_get_port_02(void **state) {
    execute_test(test_steps_mesh_get_port_02, state);
    return;
}

/* Test Steps for meshlink_get_port Test Case # 2*/
static bool test_steps_mesh_get_port_02(void) {
	bool result = false;
	int port = 0;

  meshlink_handle_t *mesh = meshlink_open("port_conf.2", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	if(!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	
	port = meshlink_get_port(NULL);
	assert(port == -1);
	if(port == -1) {
		fprintf(stderr, "meshlink_add_external_address status: %s\n", meshlink_strerror(meshlink_errno));
		result = true;
	} else {
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
	meshlink_destroy("port_conf.2");
  return result;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
int test_meshlink_get_port(void) {
		const struct CMUnitTest blackbox_get_port_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_port_01, NULL, NULL,
            (void *)&test_mesh_get_port_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_port_02, NULL, NULL,
            (void *)&test_mesh_get_port_02_state)
		};

  total_tests += sizeof(blackbox_get_port_tests) / sizeof(blackbox_get_port_tests[0]);

  return cmocka_run_group_tests(blackbox_get_port_tests, NULL, NULL);
}
