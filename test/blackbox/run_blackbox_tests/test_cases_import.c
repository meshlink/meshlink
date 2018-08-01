/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_import.c -- Execution of specific meshlink black box test cases
 * @see
 * @author    Sai Roop, sairoop@elear.solutions
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
#include "test_cases_import.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>

/*************************************************************************************
 *                          LOCAL MACROS                                             *
 *************************************************************************************/
/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG

/*************************************************************************************
 *                          LOCAL PROTOTYPES                                         *
 *************************************************************************************/
static void test_case_import_01(void **state);
static bool test_import_01(void);
static void test_case_import_02(void **state);
static bool test_import_02(void);
static void test_case_import_03(void **state);
static bool test_import_03(void);
static void test_case_import_04(void **state);
static bool test_import_04(void);
static void test_case_import_05(void **state);
static bool test_import_05(void);
static void test_case_import_06(void **state);
static bool test_import_06(void);

/*************************************************************************************
 *                          LOCAL VARIABLES                                          *
 *************************************************************************************/
/* State structure for import API Test Case #1 */
static black_box_state_t test_case_import_01_state = {
    /* test_case_name = */ "test_case_import_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for import API Test Case #2 */
static black_box_state_t test_case_import_02_state = {
    /* test_case_name = */ "test_case_import_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for import API Test Case #3 */
static black_box_state_t test_case_import_03_state = {
    /* test_case_name = */ "test_case_import_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for import API Test Case #4 */
static black_box_state_t test_case_import_04_state = {
    /* test_case_name = */ "test_case_import_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for import API Test Case #5 */
static black_box_state_t test_case_import_05_state = {
    /* test_case_name = */ "test_case_import_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for import API Test Case #6 */
static black_box_state_t test_case_import_06_state = {
    /* test_case_name = */ "test_case_import_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* Execute import Test Case # 1 - valid case*/
static void test_case_import_01(void **state) {
    execute_test(test_import_01, state);
    return;
}

/* Test Steps for meshlink_import Test Case # 1 - Valid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Export and Import mutually

    Expected Result:
    Both the nodes imports successfully
*/
static bool test_import_01(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  /* Opening NUT and bar nodes */
  fprintf(stderr, "[ import 01 ] Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ import 01 ] Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /** Exporting and Importing mutually **/
  fprintf(stderr, "[ import 01 ] Exporting NUT & bar\n");
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);

	fprintf(stderr, "[ import 01 ] Importing NUT & bar mutually \n");
	bool imp1 = meshlink_import(mesh1, exp2);
	bool imp2 = meshlink_import(mesh2, exp1);

	if(imp1 && imp2) {
    fprintf(stderr, "meshlink_import mesh1 & mesh2 imported successfully\n");
	}
	else {
    fprintf(stderr, "Failed to IMPORT mesh1 & mesh2\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return imp1 && imp2;
}

/* Execute import Test Case # 2 - invalid case*/
static void test_case_import_02(void **state) {
    execute_test(test_import_02, state);
    return;
}

/* Test Steps for meshlink_import Test Case # 2 - Invalid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Passing NULL as mesh handle argument for meshlink_import API

    Expected Result:
    Reports error successfully by returning false
*/
static bool test_import_02(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  /* Opening NUT and bar nodes */
  fprintf(stderr, "\n[ import 02 ] Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ import 02 ] Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Exporting nodes */
  fprintf(stderr, "[ import 02 ] Exporting NUT & bar\n");
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);

	fprintf(stderr, "[ import 02 ] Importing NUT with NULL as mesh handle \n");
	bool imp1 = meshlink_import(NULL, exp2);
	bool imp2 = meshlink_import(mesh2, exp1);

	if( (!imp1) && imp2 ) {
    fprintf(stderr, "meshlink_import mesh1 successfully reported error when NULL mesh handler argument error\n");
	}
	else {
    fprintf(stderr, "Failed to report NULL argument error\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return (!imp1) && imp2;
}


/* Execute import Test Case # 3 - invalid case*/
static void test_case_import_03(void **state) {
    execute_test(test_import_03, state);
    return;
}

/* Test Steps for meshlink_import Test Case # 3 - Invalid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Passing NULL as exported data argument for meshlink_import API

    Expected Result:
    Reports error successfully by returning false
*/
static bool test_import_03(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  /* Opening NUT and bar nodes */
  fprintf(stderr, "\n[ import 03 ] Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ import 03 ] Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Exporting nodes */
  fprintf(stderr, "[ import 03 ] Exporting NUT & bar\n");
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);

	fprintf(stderr, "[ import 03 ] Importing NUT with NULL as exported data argument \n");
	bool imp1 = meshlink_import(mesh1, NULL);
	bool imp2 = meshlink_import(mesh2, exp1);

	if( (!imp1) && imp2 ) {
    fprintf(stderr, "meshlink_import mesh1 successfully reported error when NULL is passed as exported data argument\n");
	}
	else {
    fprintf(stderr, "Failed to report NULL argument error\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return (!imp1) && imp2;
}

/* Execute import Test Case # 4 - invalid case garbage string*/
static void test_case_import_04(void **state) {
    execute_test(test_import_04, state);
    return;
}

/* Test Steps for meshlink_import Test Case # 4 - Invalid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Passing some garbage string(NULL terminated)
        as an argument for meshlink_import API

    Expected Result:
    Reports error successfully by returning false
*/
static bool test_import_04(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  /* Opening NUT and bar nodes */
  fprintf(stderr, "\n[ import 04 ] Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ import 04 ] Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Exporting nodes */
  fprintf(stderr, "[ import 04 ] Exporting NUT & bar\n");
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);

	fprintf(stderr, "[ import 04 ] Importing NUT with garbage string as exported data argument \n");
	bool imp1 = meshlink_import(mesh1, "1/2/3");
	bool imp2 = meshlink_import(mesh2, exp1);

	if( (!imp1) && imp2 ) {
    fprintf(stderr, "meshlink_import mesh1 successfully reported error when a garbage string is passed as exported data argument\n");
	}
	else {
    fprintf(stderr, "Failed to report error when a garbage string is used for importing meta data\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return (!imp1) && imp2;
}

/* Execute import Test Case # 5 - valid case*/
static void test_case_import_05(void **state) {
    execute_test(test_import_05, state);
    return;
}

/* Test Steps for meshlink_import Test Case # 5 - Invalid case

    Test Steps:
    1. Open NUT(Node Under Test) & bar meshes.
    2. Export and Import mutually
    2. Try to import NUT again/twice at 'bar' node

    Expected Result:
    Reports error successfully by returning false
*/
static bool test_import_05(void) {
  meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  /* Opening NUT and bar nodes */
  fprintf(stderr, "\n[ import 05 ] Opening NUT\n");
  meshlink_handle_t *mesh1 = meshlink_open("importconf1", "nut", "chat", DEV_CLASS_STATIONARY);
	assert(mesh1 != NULL);
	if(!mesh1) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh1, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  fprintf(stderr, "[ import 05 ] Opening bar\n");
  meshlink_handle_t *mesh2 = meshlink_open("importconf2", "bar", "chat", DEV_CLASS_STATIONARY);
	assert(mesh2 != NULL);
	if(!mesh2) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
  /* Set up logging for Meshlink */
  meshlink_set_log_cb(mesh2, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

  /* Exporting nodes */
  fprintf(stderr, "[ import 05 ] Exporting NUT & bar\n");
	char *exp1 = meshlink_export(mesh1);
	assert(exp1 != NULL);
	char *exp2 = meshlink_export(mesh2);
	assert(exp2 != NULL);

	fprintf(stderr, "[ import 05 ] Importing NUT & bar\n");
	bool imp1 = meshlink_import(mesh1, exp2);
	assert(imp1);
	bool imp2 = meshlink_import(mesh2, exp1);
	assert(imp2);

	/** Trying to import twice **/
	fprintf(stderr, "[ import 05 ] trying to import twice \n");
	bool imp3 = meshlink_import(mesh2, exp1);

	if(imp3) {
    fprintf(stderr, "meshlink_import when imported twice returned 'true'\n");
	}
	else {
    fprintf(stderr, "meshlink_import when imported twice returned 'false'\n");
	}

	meshlink_close(mesh1);
	meshlink_close(mesh2);
	meshlink_destroy("importconf1");
	meshlink_destroy("importconf2");
  return !imp3;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
int test_meshlink_import(void) {
  const struct CMUnitTest blackbox_import_tests[] = {
      cmocka_unit_test_prestate_setup_teardown(test_case_import_01, NULL, NULL,
            (void *)&test_case_import_01_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_import_02, NULL, NULL,
            (void *)&test_case_import_02_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_import_03, NULL, NULL,
            (void *)&test_case_import_03_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_import_04, NULL, NULL,
            (void *)&test_case_import_04_state),
      cmocka_unit_test_prestate_setup_teardown(test_case_import_05, NULL, NULL,
            (void *)&test_case_import_05_state)
  };
  total_tests += sizeof(blackbox_import_tests) / sizeof(blackbox_import_tests[0]);

  return cmocka_run_group_tests(blackbox_import_tests ,NULL , NULL);
}
