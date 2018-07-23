/*
    test_cases_start.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_start.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>

/* Execute meshlink_start Test Case # 1*/
void test_case_mesh_start_01(void **state) {
	 execute_test(test_steps_mesh_start_01, state);
   return;
}

/* Test Steps for meshlink_start Test Case # 1*/
bool test_steps_mesh_start_01(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("start_conf", "foo", "chat", DEV_CLASS_STATIONARY);
		assert(mesh != NULL);
		result = meshlink_start(mesh);
		if (!result) {
			fprintf(stderr, "meshlink_start status1: %s\n", meshlink_strerror(meshlink_errno));
			return false;	
		} else {
			result = true;
		}
		assert(result != false);
    return result;
}

/* Execute meshlink_start Test Case # 2*/
void test_case_mesh_start_02(void **state) {
	 execute_test(test_steps_mesh_start_02, state);
   return;
}

/* Test Steps for meshlink_start Test Case # 2*/
bool test_steps_mesh_start_02(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("start_conf", "foo", "chat", DEV_CLASS_STATIONARY);
		assert(mesh != NULL);	
		result = meshlink_start(NULL);
		if (!result) {
			fprintf(stderr, "meshlink_start status 2: %s\n", meshlink_strerror(meshlink_errno));
			return true;
		} else {
			result = false;
		}
		assert(result != false);	
    return result;
}

