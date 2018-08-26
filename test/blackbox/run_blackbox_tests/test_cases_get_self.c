/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_get_self.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_get_self.h"
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
static void test_case_mesh_get_self_01(void **state);
static bool test_steps_mesh_get_self_01(void);
static void test_case_mesh_get_self_02(void **state);
static bool test_steps_mesh_get_self_02(void);

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* State structure for meshlink_get_self Test Case #1 */
static black_box_state_t test_mesh_get_self_01_state = {
    /* test_case_name = */ "test_case_mesh_get_self_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_self Test Case #2 */
static black_box_state_t test_mesh_get_self_02_state = {
    /* test_case_name = */ "test_case_mesh_get_self_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/

/* Execute meshlink_get_self Test Case # 1 */
static void test_case_mesh_get_self_01(void **state) {
    execute_test(test_steps_mesh_get_self_01, state);
    return;
}

/* Test Steps for meshlink_get_self Test Case # 1*/
static bool test_steps_mesh_get_self_01(void) {
	bool result = false;
  meshlink_handle_t *mesh1 = NULL;
	meshlink_node_t *dest_node = NULL;
	int pmtu;
	mesh1 = meshlink_open("self_conf.1", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	if(!meshlink_start(mesh1)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;	
	}
	dest_node = meshlink_get_self(mesh1);
	assert(dest_node != NULL);
	if(!dest_node) {
		fprintf(stderr, "meshlink_get_self status2: %s\n", meshlink_strerror(meshlink_errno));
		result = true;
	} else {
		result = false;
	}
	if(!strcmp(dest_node->name, "foo")) {
		fprintf(stderr, "Foo thinks its name is %s\n", dest_node->name);
		result = true;
	}
	meshlink_stop(mesh1);
	meshlink_close(mesh1);
	meshlink_destroy("self_conf.1");
  return result;

}

/* Execute meshlink_get_self Test Case # 2 */
static void test_case_mesh_get_self_02(void **state) {
    execute_test(test_steps_mesh_get_self_02, state);
    return;
}

/* Test Steps for meshlink_get_self Test Case # 2*/
static bool test_steps_mesh_get_self_02(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	int pmtu;
	mesh = meshlink_open("self_conf.2", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	if (!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	dest_node = meshlink_get_self(NULL);
	assert(mesh != NULL);
	if(!dest_node) {
		fprintf(stderr, "meshlink_get_self status2: %s\n", meshlink_strerror(meshlink_errno));
		result = true;
	} else {
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
	meshlink_destroy("self_conf.2");
  return result;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
int test_meshlink_get_self(void) {
		const struct CMUnitTest blackbox_get_self_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_self_01, NULL, NULL,
            (void *)&test_mesh_get_self_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_self_02, NULL, NULL,
            (void *)&test_mesh_get_self_02_state)
		};

  total_tests += sizeof(blackbox_get_self_tests) / sizeof(blackbox_get_self_tests[0]);

  return cmocka_run_group_tests(blackbox_get_self_tests, NULL, NULL);
}
