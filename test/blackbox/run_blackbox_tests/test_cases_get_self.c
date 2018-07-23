/*
    test_cases_get_self.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_get_self.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>

/* Execute meshlink_get_self Test Case # 1 */
void test_case_mesh_get_self_01(void **state) {
    execute_test(test_steps_mesh_get_self_01, state);
    return;
}

/* Test Steps for meshlink_get_self Test Case # 1*/
bool test_steps_mesh_get_self_01(void) {
	bool result = false;
  meshlink_handle_t *mesh1 = NULL;
	meshlink_node_t *dest_node = NULL;
	int pmtu;
	mesh1 = meshlink_open("pmtu_conf", "foo", "chat", DEV_CLASS_STATIONARY);
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
		return true;
	} else {
		result = false;
	}
	if(!strcmp(dest_node->name, "foo")) {
		fprintf(stderr, "Foo thinks its name is %s\n", dest_node->name);
		return true;
	}
	meshlink_stop(mesh1);
	meshlink_close(mesh1);
  return result;

}

/* Execute meshlink_get_self Test Case # 2 */
void test_case_mesh_get_self_02(void **state) {
    execute_test(test_steps_mesh_get_self_02, state);
    return;
}

/* Test Steps for meshlink_get_self Test Case # 2*/
bool test_steps_mesh_get_self_02(void) {
	bool result = false;
  meshlink_handle_t *mesh = NULL;
	meshlink_node_t *dest_node = NULL;
	int pmtu;
	mesh = meshlink_open("pmtu_conf", "foo", "chat", DEV_CLASS_STATIONARY);
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
		return true;
	} else {
		result = false;
	}

	meshlink_stop(mesh);
	meshlink_close(mesh);
  return result;
}

