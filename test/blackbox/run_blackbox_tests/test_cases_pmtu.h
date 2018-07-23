/*
    test_cases_pmtu.h -- Declarations for Individual Test Case implementation functions
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

#ifndef TEST_CASES_PMTU_H
#define TEST_CASES_PMTU_H

#include <stdbool.h>

void test_case_mesh_pmtu_01(void **state);
bool test_steps_mesh_pmtu_01(void);
void test_case_mesh_pmtu_02(void **state);
bool test_steps_mesh_pmtu_02(void);
void test_case_mesh_pmtu_03(void **state);
bool test_steps_mesh_pmtu_03(void);

#endif
