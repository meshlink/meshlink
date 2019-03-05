#ifndef EXECUTE_TESTS_H
#define EXECUTE_TESTS_H

/*
    execute_tests.h -- header file for execute_tests.c
    Copyright (C) 2018  Guus Sliepen <guus@meshlink.io>

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

#include <stdbool.h>
#include "../common/mesh_event_handler.h"

typedef struct {
	const mesh_event_t              *expected_events;
	int                             current_index;
	int                             max_events;
} node_status_t;

typedef bool (*test_step_func_t)(void);

int setup_test(void **state);
void execute_test(test_step_func_t step_func, void **state);
int teardown_test(void **state);

/// Changes the state of the node state machine.
/** This function changes the current state of the node
 *
 *  @param status           Pointer to status handle of that node.
 *  @param currentEv        Current event triggered by the node.
 *
 *  @return                 This function returns true if state change is successful else returns false
 */
extern bool change_state(node_status_t *status, mesh_event_t currentEv);

/// Sends SIGIO signal to all the nodes in the container.
/** This function Triggers SIGIO signal to all the target applications running inside the container
 *
 *  @param status           Pointer to array of status handles of target nodes.
 *  @param start            Starting index from which to start in the array.
 *  @param end              Ending index of the array
 *  @param node_ids         Pointer to array of node id strings
 *
 *  @return                 Void
 */
extern void signal_node_start(node_status_t *node_status, int start, int end, char *node_ids[]);

/// Checks for the completion of nodes state machines.
/** This function checks wheather the nodes state machines have reached their maximum state indexes
 *
 *  @param status           Pointer to array of status handles of target nodes.
 *  @param length               Number of nodes to check.
 *
 *  @return                 This function returns true if all the nodes reached their max states
 */
extern bool check_nodes_finished(node_status_t *node_status, int length);

#endif // TEST_STEP_H
