/*
    test_cases_stop_close.c -- Execution of specific meshlink black box test cases
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
#include "test_cases_stop_close.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>

/* Execute meshlink_close Test Case # 1*/
void test_case_mesh_close_01(void **state) {
	 execute_test(test_steps_mesh_close_01, state);
   return;
}

/* Test Steps for meshlink_close Test Case # 1*/
bool test_steps_mesh_close_01(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("start_conf", "foo", "chat", DEV_CLASS_STATIONARY);
		assert(mesh != NULL);
		result = meshlink_start(mesh);
		if (!result) {
			fprintf(stderr, "meshlink_start status 1: %s\n", meshlink_strerror(meshlink_errno));
			return false;
		} else {
			result = true;
		}
		assert(result != false);
		meshlink_close(mesh);
    return result;
}

/* Execute meshlink_stop Test Case # 1*/
void test_case_mesh_stop_01(void **state) {
	 execute_test(test_steps_mesh_stop_01, state);
   return;
}

/* Test Steps for meshlink_stop Test Case # 1*/
bool test_steps_mesh_stop_01(void) {
		bool result = false;
		meshlink_handle_t *mesh = NULL;

    mesh = meshlink_open("start_conf", "foo", "chat", DEV_CLASS_STATIONARY);
		assert(mesh != NULL);
		result = meshlink_start(mesh);
		if (!result) {
			fprintf(stderr, "meshlink_start status 2: %s\n", meshlink_strerror(meshlink_errno));
			return false;
		} else {
			result = true;
		}
		assert(result != false);
		meshlink_stop(mesh);
    return result;
}

