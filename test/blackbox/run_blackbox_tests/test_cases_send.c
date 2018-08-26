/*===================================================================================*/
/*************************************************************************************/
/**
 * @file      test_cases_send.c -- Execution of specific meshlink black box test cases
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

/*************************************************************************************
 *                          LOCAL MACROS                                             *
 *************************************************************************************/

/*************************************************************************************
 *                          LOCAL PROTOTYPES                                         *
 *************************************************************************************/
static void test_case_mesh_send_01(void **state);
static bool test_steps_mesh_send_01(void);
static void test_case_mesh_send_02(void **state);
static bool test_steps_mesh_send_02(void);
static void test_case_mesh_send_03(void **state);
static bool test_steps_mesh_send_03(void);
static void test_case_mesh_send_04(void **state);
static bool test_steps_mesh_send_04(void);
static void test_case_mesh_send_05(void **state);
static bool test_steps_mesh_send_05(void);
static void test_case_mesh_send_06(void **state);
static bool test_steps_mesh_send_06(void);

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/
/* State structure for meshlink_send Test Case #1 */
static black_box_state_t test_mesh_send_01_state = {
    /* test_case_name = */ "test_case_mesh_send_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #2 */
static black_box_state_t test_mesh_send_02_state = {
    /* test_case_name = */ "test_case_mesh_send_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #3 */
static black_box_state_t test_mesh_send_03_state = {
    /* test_case_name = */ "test_case_mesh_send_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #4 */
static black_box_state_t test_mesh_send_04_state = {
    /* test_case_name = */ "test_case_mesh_send_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #5 */
static black_box_state_t test_mesh_send_05_state = {
    /* test_case_name = */ "test_case_mesh_send_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #6 */
static black_box_state_t test_mesh_send_06_state = {
    /* test_case_name = */ "test_case_mesh_send_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/*************************************************************************************
 *                          PRIVATE FUNCTIONS                                        *
 *************************************************************************************/

/* Execute meshlink_send Test Case # 1 */
static void test_case_mesh_send_01(void **state) {
    execute_test(test_steps_mesh_send_01, state);
    return;
}

static void receive(meshlink_handle_t *mesh, meshlink_node_t *dest_node, const void *data, size_t len) {
	const char *msg = data;

	if(!len) {
		fprintf(stderr, "Received invalid data from %s\n", dest_node->name);
		return;
	}

	fprintf(stderr, "%s says: %s\n", dest_node->name, msg);
}

/* Test Steps for meshlink_send Test Case # 1*/
static bool test_steps_mesh_send_01(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	mesh = meshlink_open("send_conf.1", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);

	meshlink_set_receive_cb(mesh, receive);

	if (!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);
	dest_node = meshlink_get_self(mesh);
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
	meshlink_stop(mesh);
	meshlink_destroy("send_conf.1");
  return result;
}

/* Execute meshlink_send Test Case # 2 */
static void test_case_mesh_send_02(void **state) {
    execute_test(test_steps_mesh_send_02, state);
    return;
}

/* Test Steps for meshlink_send Test Case # 2*/
static bool test_steps_mesh_send_02(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	mesh = meshlink_open("send_conf.2", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}

	meshlink_set_receive_cb(mesh, receive);

	if (!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);
	assert(meshlink_start(mesh) != false);
	dest_node = meshlink_get_self(mesh);
	if(!dest_node) {
		fprintf(stderr, "meshlink_get_self status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	assert(dest_node != NULL);
	
	result = meshlink_send(NULL, dest_node, data, len);
	assert(result != true	);
	if(!result) {
		fprintf(stderr, "meshlink_send status: %s\n", meshlink_strerror(meshlink_errno));
		result = true;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = false;
	}

	meshlink_close(mesh);
	meshlink_stop(mesh);
	meshlink_destroy("send_conf.2");
  return result;
}

/* Execute meshlink_send Test Case # 3 */
static void test_case_mesh_send_03(void **state) {
    execute_test(test_steps_mesh_send_03, state);
    return;
}

/* Test Steps for meshlink_send Test Case # 3*/
static bool test_steps_mesh_send_03(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	mesh = meshlink_open("send_conf.3", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}

	meshlink_set_receive_cb(mesh, receive);

	if (!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);
	dest_node = meshlink_get_self(mesh);
	assert(dest_node != NULL);	
	if(!dest_node) {
		fprintf(stderr, "meshlink_get_self status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	
	result = meshlink_send(mesh, NULL, data, len);
	assert(result != true);
	if(!result) {
		fprintf(stderr, "meshlink_send status3: %s\n", meshlink_strerror(meshlink_errno));
		result = true;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
	meshlink_destroy("send_conf.3");
  return result;
}

/* Execute meshlink_send Test Case # 4 */
static void test_case_mesh_send_04(void **state) {
    execute_test(test_steps_mesh_send_04, state);
    return;
}
static void receive4(meshlink_handle_t *mesh, meshlink_node_t *dest_node, const void *data, size_t len) {
	const char *msg = data;

	if(!len) {
		fprintf(stderr, "Received invalid data from %s\n", dest_node->name);
		return;
	}

	fprintf(stderr, "%s says: %s\n", dest_node->name, msg);
}
/* Test Steps for meshlink_send Test Case # 4*/
static bool test_steps_mesh_send_04(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;

	mesh = meshlink_open("send_conf.4", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}

	meshlink_set_receive_cb(mesh, receive);

	if (!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);
	dest_node = meshlink_get_self(mesh);
	assert(dest_node != NULL);
	if(!dest_node) {
		fprintf(stderr, "meshlink_get_self status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	
	result = meshlink_send(mesh, dest_node, NULL, 0);
	sleep(2);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status4: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = true;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
	meshlink_destroy("send_conf.4");
  return result;
}

/* Execute meshlink_send Test Case # 5 */
static void test_case_mesh_send_05(void **state) {
    execute_test(test_steps_mesh_send_05, state);
    return;
}
static void receive5(meshlink_handle_t *mesh, meshlink_node_t *dest_node, const void *data, size_t len) {
	const char *msg = data;

	if(!len) {
		fprintf(stderr, "Received invalid data from %s\n", dest_node->name);
		return;
	}

	fprintf(stderr, "%s says: %s\n", dest_node->name, msg);
}
/* Test Steps for meshlink_send Test Case # 5*/
static bool test_steps_mesh_send_05(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;

	mesh = meshlink_open("send_conf.5", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}

	meshlink_set_receive_cb(mesh, receive5);

	if (!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);
	dest_node = meshlink_get_self(mesh);
	assert(dest_node != NULL);
	if(!dest_node) {
		fprintf(stderr, "meshlink_get_self status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	
	result = meshlink_send(mesh, dest_node, data, 0);
	assert(result != false);
	if(!result) {
		fprintf(stderr, "meshlink_send status5: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = true;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
	meshlink_destroy("send_conf.5");
  return result;
}

/* Execute meshlink_send Test Case # 6 */
static void test_case_mesh_send_06(void **state) {
    execute_test(test_steps_mesh_send_06, state);
    return;
}

/* Test Steps for meshlink_send Test Case # 6*/
static bool test_steps_mesh_send_06(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	mesh = meshlink_open("send_conf.6", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}

	meshlink_set_receive_cb(mesh, receive);

	if (!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	sleep(2);
	dest_node = meshlink_get_self(mesh);
	assert(dest_node != NULL);
	if(!dest_node) {
		fprintf(stderr, "meshlink_get_self status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	
	result = meshlink_send(mesh, dest_node, NULL, len);
	assert(result != true);
	if(!result) {
		fprintf(stderr, "meshlink_send status6: %s\n", meshlink_strerror(meshlink_errno));
		result = true;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
	meshlink_destroy("send_conf.6");
  return result;
}

/*************************************************************************************
 *                          PUBLIC FUNCTIONS                                         *
 *************************************************************************************/
int test_meshlink_send(void) {
		const struct CMUnitTest blackbox_send_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_01, NULL, NULL,
            (void *)&test_mesh_send_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_02, NULL, NULL,
            (void *)&test_mesh_send_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_03, NULL, NULL,
            (void *)&test_mesh_send_03_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_04, NULL, NULL,
            (void *)&test_mesh_send_04_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_05, NULL, NULL,
            (void *)&test_mesh_send_05_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_06, NULL, NULL,
            (void *)&test_mesh_send_06_state)
		};

  total_tests += sizeof(blackbox_send_tests) / sizeof(blackbox_send_tests[0]);

  return cmocka_run_group_tests(blackbox_send_tests, NULL, NULL);
}
