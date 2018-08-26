/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_open.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_open.h"
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
static void test_case_mesh_open_01(void **state);
static bool test_steps_mesh_open_01(void);
static void test_case_mesh_open_02(void **state);
static bool test_steps_mesh_open_02(void);
static void test_case_mesh_open_03(void **state);
static bool test_steps_mesh_open_03(void);
static void test_case_mesh_open_04(void **state);
static bool test_steps_mesh_open_04(void);
static void test_case_mesh_open_05(void **state);
static bool test_steps_mesh_open_05(void);

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* State structure for meshlink_open Test Case #1 */
static black_box_state_t test_mesh_open_01_state = {
    /* test_case_name = */ "test_case_mesh_open_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #2 */
static black_box_state_t test_mesh_open_02_state = {
    /* test_case_name = */ "test_case_mesh_open_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #3 */
static black_box_state_t test_mesh_open_03_state = {
    /* test_case_name = */ "test_case_mesh_open_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #4 */
static black_box_state_t test_mesh_open_04_state = {
    /* test_case_name = */ "test_case_mesh_open_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #5 */
static black_box_state_t test_mesh_open_05_state = {
    /* test_case_name = */ "test_case_mesh_open_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* Execute meshlink_open Test Case # 1*/
static void test_case_mesh_open_01(void **state) {
	 execute_test(test_steps_mesh_open_01, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 1*/
static bool test_steps_mesh_open_01(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;
		const char *confbase = "open_conf.1";

    mesh = meshlink_open(confbase, "foo", "chat", DEV_CLASS_STATIONARY);
		if (!mesh) {
			fprintf(stderr, "meshlink_open status1: %s\n", meshlink_strerror(meshlink_errno));
			return false;
		} else {
			result = true;
		}
		assert(mesh != NULL);
		meshlink_destroy("open_conf.1");
    return result;
}

/* Execute meshlink_open Test Case # 2*/
static void test_case_mesh_open_02(void **state) {
	 execute_test(test_steps_mesh_open_02, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 2*/
static bool test_steps_mesh_open_02(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

		mesh = meshlink_open(NULL, "foo", "chat", DEV_CLASS_STATIONARY);
		assert(mesh == NULL);
		if(!mesh) {
			fprintf(stderr, "meshlink_open status 2: %s\n", meshlink_strerror(meshlink_errno));
			return true;
		} else {
			result = false;
		}
		meshlink_destroy("open_conf.2");
    return result;
}

/* Execute meshlink_open Test Case # 3*/
static void test_case_mesh_open_03(void **state) {
	 execute_test(test_steps_mesh_open_03, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 3*/
static bool test_steps_mesh_open_03(void) {
		bool result = false;
		const char *confbase = "open_conf.3";
		const char *name = NULL;
		meshlink_handle_t *mesh = meshlink_open(".chat", name, "chat", DEV_CLASS_STATIONARY);
		assert(mesh == NULL);
		if(!mesh) {
			fprintf(stderr, "meshlink_open status3: %s\n", meshlink_strerror(meshlink_errno));
			return true;
		} else {
			result = false;
		}
		meshlink_destroy("open_conf.3");
    return result;
}

/* Execute meshlink_open Test Case # 4*/
static void test_case_mesh_open_04(void **state) {
	 execute_test(test_steps_mesh_open_04, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 4*/
static bool test_steps_mesh_open_04(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("open_conf.4", "foo", NULL, DEV_CLASS_STATIONARY);
		assert(mesh == NULL);
		if (!mesh) {
			fprintf(stderr, "meshlink_open status 4: %s\n", meshlink_strerror(meshlink_errno));
			return true;
		} else {
			result = false;
		}
		meshlink_destroy("open_conf.4");
    return result;
}

/* Execute meshlink_open Test Case # 5*/
static void test_case_mesh_open_05(void **state) {
	 execute_test(test_steps_mesh_open_05, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 5*/
static bool test_steps_mesh_open_05(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("open_conf.5", "foo", "chat", -1);
		assert(mesh == NULL);
		if (!mesh) {
			fprintf(stderr, "meshlink_open status 5: %s\n", meshlink_strerror(meshlink_errno));
			return true;		
		} else {
			result = false;
		}	
		meshlink_destroy("open_conf.5");
    return result;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
int test_meshlink_open(void) {
		const struct CMUnitTest blackbox_open_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_01, NULL, NULL,
            (void *)&test_mesh_open_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_02, NULL, NULL,
            (void *)&test_mesh_open_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_03, NULL, NULL,
            (void *)&test_mesh_open_03_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_04, NULL, NULL,
            (void *)&test_mesh_open_04_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_05, NULL, NULL,
            (void *)&test_mesh_open_05_state)

		};

  total_tests += sizeof(blackbox_open_tests) / sizeof(blackbox_open_tests[0]);

  return cmocka_run_group_tests(blackbox_open_tests, NULL, NULL);
}
