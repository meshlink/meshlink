/*
    execute_tests.c -- Utility functions for black box test execution
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

#include <stdlib.h>
#include <stdarg.h>
#include <setjmp.h>
#include <cmocka.h>
#include <assert.h>
#include "execute_tests.h"
#include "../common/common_handlers.h"
#include "../common/containers.h"
#include "../common/test_step.h"

int setup_test(void **state) {
    int i;

    printf("Setting up Containers\n");
    state_ptr = (black_box_state_t *)(*state);
    for(i = 0; i < state_ptr->num_nodes; i++) {
        meta_conn_status[i] = false;
        node_reachable_status[i] = false;
    }

    setup_containers(state);

    return EXIT_SUCCESS;
}

void execute_test(test_step_func_t step_func, void **state) {
    black_box_state_t *test_state = (black_box_state_t *) (*state);

    printf("Running Test\n");
    test_state->test_result = step_func();

    if(!test_state->test_result)
        fail();

    return;
}

int teardown_test(void **state) {
    black_box_state_t *test_state = (black_box_state_t *) (*state);
    char container_old_name[100], container_new_name[100];
    int i;

    if(test_state->test_result) {
        PRINT_TEST_CASE_MSG("Test successful! Shutting down nodes.\n");
        for(i = 0; i < test_state->num_nodes; i++) {
            /* Shut down node */
            node_step_in_container(test_state->node_names[i], "SIGTERM");
            /* Rename Container to run_<node-name> - this allows it to be re-used for the
                next test, otherwise it will be ignored assuming that it has been saved
                for debugging */
            assert(snprintf(container_old_name, sizeof(container_old_name), "%s_%s",
                test_state->test_case_name, test_state->node_names[i]) >= 0);
            assert(snprintf(container_new_name, sizeof(container_new_name), "run_%s",
                test_state->node_names[i]) >= 0);
            rename_container(container_old_name, container_new_name);
        }
    }

    PRINT_TEST_CASE_MSG("Terminating NUT.\n");
    execute_close();
    state_ptr = NULL;

    return EXIT_SUCCESS;
}
