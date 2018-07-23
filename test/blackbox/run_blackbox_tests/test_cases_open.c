/*
    test_cases.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_open.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>

/* Execute meshlink_open Test Case # 1*/
void test_case_mesh_open_01(void **state) {
	 execute_test(test_steps_mesh_open_01, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 1*/
bool test_steps_mesh_open_01(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;
		const char *confbase = "open_conf";

    mesh = meshlink_open(confbase, "foo", "chat", DEV_CLASS_STATIONARY);
		if (!mesh) {
			fprintf(stderr, "meshlink_open status1: %s\n", meshlink_strerror(meshlink_errno));
			return false;
		} else {
			result = true;
		}
		assert(mesh != NULL);
    return result;
}

/* Execute meshlink_open Test Case # 2*/
void test_case_mesh_open_02(void **state) {
	 execute_test(test_steps_mesh_open_02, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 2*/
bool test_steps_mesh_open_02(void) {
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

    return result;
}

/* Execute meshlink_open Test Case # 3*/
void test_case_mesh_open_03(void **state) {
	 execute_test(test_steps_mesh_open_03, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 3*/
bool test_steps_mesh_open_03(void) {
		bool result = false;
		const char *confbase = "open_conf";
		const char *name = NULL;
		meshlink_handle_t *mesh = meshlink_open(".chat", name, "chat", DEV_CLASS_STATIONARY);
		assert(mesh == NULL);
		if(!mesh) {
			fprintf(stderr, "meshlink_open status3: %s\n", meshlink_strerror(meshlink_errno));
			return true;
		} else {
			result = false;
		}
    return result;
}

/* Execute meshlink_open Test Case # 4*/
void test_case_mesh_open_04(void **state) {
	 execute_test(test_steps_mesh_open_04, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 4*/
bool test_steps_mesh_open_04(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("open_conf", "foo", NULL, DEV_CLASS_STATIONARY);
		assert(mesh == NULL);
		if (!mesh) {
			fprintf(stderr, "meshlink_open status 4: %s\n", meshlink_strerror(meshlink_errno));
			return true;
		} else {
			result = false;
		}
    return result;
}

/* Execute meshlink_open Test Case # 5*/
void test_case_mesh_open_05(void **state) {
	 execute_test(test_steps_mesh_open_05, state);
   return;
}

/* Test Steps for meshlink_open Test Case # 5*/
bool test_steps_mesh_open_05(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("open_conf", "foo", "chat", -1);
		assert(mesh == NULL);
		if (!mesh) {
			fprintf(stderr, "meshlink_open status 5: %s\n", meshlink_strerror(meshlink_errno));
			return true;		
		} else {
			result = false;
		}	
    return result;
}

