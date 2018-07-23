/*
    test_cases_send.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

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
#include "test_cases_send.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>

/* Execute meshlink_send Test Case # 1 */
void test_case_mesh_send_01(void **state) {
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
bool test_steps_mesh_send_01(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	mesh = meshlink_open("send_conf", "foo", "chat", DEV_CLASS_STATIONARY);
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
  return result;
}

/* Execute meshlink_send Test Case # 2 */
void test_case_mesh_send_02(void **state) {
    execute_test(test_steps_mesh_send_02, state);
    return;
}

/* Test Steps for meshlink_send Test Case # 2*/
bool test_steps_mesh_send_02(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	mesh = meshlink_open("send_conf", "foo", "chat", DEV_CLASS_STATIONARY);
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
		return true;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = false;
	}

	meshlink_close(mesh);
	meshlink_stop(mesh);

  return result;
}

/* Execute meshlink_send Test Case # 3 */
void test_case_mesh_send_03(void **state) {
    execute_test(test_steps_mesh_send_03, state);
    return;
}

/* Test Steps for meshlink_send Test Case # 3*/
bool test_steps_mesh_send_03(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	mesh = meshlink_open("send_conf", "foo", "chat", DEV_CLASS_STATIONARY);
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
		return true;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
  return result;
}

/* Execute meshlink_send Test Case # 4 */
void test_case_mesh_send_04(void **state) {
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
bool test_steps_mesh_send_04(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;

	mesh = meshlink_open("send_conf", "foo", "chat", DEV_CLASS_STATIONARY);
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
  return result;
}

/* Execute meshlink_send Test Case # 5 */
void test_case_mesh_send_05(void **state) {
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
bool test_steps_mesh_send_05(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;

	mesh = meshlink_open("send_conf", "foo", "chat", DEV_CLASS_STATIONARY);
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
  return result;
}

/* Execute meshlink_send Test Case # 6 */
void test_case_mesh_send_06(void **state) {
    execute_test(test_steps_mesh_send_06, state);
    return;
}

/* Test Steps for meshlink_send Test Case # 6*/
bool test_steps_mesh_send_06(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	char *data = NULL;
	size_t len;
	char buf[] = "hello";

	data = buf;
	len = sizeof(buf);

	mesh = meshlink_open("send_conf", "foo", "chat", DEV_CLASS_STATIONARY);
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
		return true;
	} else {
		fprintf(stderr, "data %s sent to %s node\n", data, dest_node->name);
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
  return result;
}

