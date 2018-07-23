/*
    test_cases_get_node.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_get_node.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>

/* Execute meshlink_get_node Test Case # 1 */
void test_case_mesh_get_node_01(void **state) {
    execute_test(test_steps_mesh_get_node_01, state);
    return;
}

/* Test Steps for meshlink_get_node Test Case # 1*/
bool test_steps_mesh_get_node_01(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *get_node = NULL;
	int pmtu;
	mesh = meshlink_open("pmtu_conf", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	if(!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;	
	}

	get_node = meshlink_get_node(mesh, "foo");
	assert(get_node != NULL);
	if(!get_node) {
		fprintf(stderr, "meshlink_get_self status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	} else {
		result = true;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
  return result;
}

/* Execute meshlink_get_node Test Case # 2 */
void test_case_mesh_get_node_02(void **state) {
    execute_test(test_steps_mesh_get_node_02, state);
    return;
}

/* Test Steps for meshlink_get_node Test Case # 2*/
bool test_steps_mesh_get_node_02(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *get_node = NULL;
	int pmtu;
	mesh = meshlink_open("pmtu_conf", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	if(!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}

	get_node = meshlink_get_node(NULL, "foo");
	assert(get_node == NULL);
	if(!get_node) {
		fprintf(stderr, "meshlink_get_self status2: %s\n", meshlink_strerror(meshlink_errno));
		return true;
	} else {
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
  return result;
}

/* Execute meshlink_get_node Test Case # 3 */
void test_case_mesh_get_node_03(void **state) {
    execute_test(test_steps_mesh_get_node_03, state);
    return;
}

/* Test Steps for meshlink_get_node Test Case # 3*/
bool test_steps_mesh_get_node_03(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *get_node = NULL;

	mesh = meshlink_open("pmtu_conf", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	if(!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}

	get_node = meshlink_get_node(mesh, NULL);
	assert(get_node == NULL);
	if(!get_node) {
		fprintf(stderr, "meshlink_get_self status2: %s\n", meshlink_strerror(meshlink_errno));
		return true;
	} else {
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
  return result;
}

/* Execute meshlink_get_node Test Case # 4 */
void test_case_mesh_get_node_04(void **state) {
    execute_test(test_steps_mesh_get_node_04, state);
    return;
}

/* Test Steps for meshlink_get_node Test Case # 4*/
bool test_steps_mesh_get_node_04(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *get_node = NULL;
	const char *unknown_name;

	mesh = meshlink_open("pmtu_conf", "foo", "chat", DEV_CLASS_STATIONARY);
	assert(mesh != NULL);
	if(!mesh) {
		fprintf(stderr, "meshlink_open status2: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}
	if(!meshlink_start(mesh)) {
		fprintf(stderr, "meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
		return false;
	}

	get_node = meshlink_get_node(mesh, unknown_name);
	assert(get_node == NULL);
	if(!get_node) {
		fprintf(stderr, "meshlink_get_self status2: %s\n", meshlink_strerror(meshlink_errno));
		return true;
	} else {
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
  return result;
}
