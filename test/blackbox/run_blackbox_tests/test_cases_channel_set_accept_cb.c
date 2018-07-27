
/*
    test_cases_add_ex_addr.c -- Execution of specific meshlink black box test cases
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>
                        Manav Kumar Mehta <manavkumarm@yahoo.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty o
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "execute_tests.h"
#include "test_cases_set_channel_accpet_cb.h"
#include "../common/containers.h"
#include "../common/test_step.h"
#include "../common/common_handlers.h"
#include <assert.h>
#include <string.h>
/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_DEBUG
static bool channel_acc;
static bool channel_accept(meshlink_handle_t *mesh, meshlink_channel_t *channel, uint16_t port, const void *data, size_t len) {
	(void)data;
	(void)len;

	channel_acc = true;

	fprintf(stderr, "Accepted incoming channel from '%s'\n", channel->node->name);

	// Accept this channel
	return true;
}

/* Execute meshlink_channel_open_ex Test Case # 1 */
void test_case_set_channel_accept_cb_01(void **state) {
    execute_test(test_steps_set_channel_accept_cb_01, state);
    return;
}

bool test_steps_set_channel_accept_cb_01(void) {
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Create meshlink instance */
    meshlink_handle_t *mesh_handle = meshlink_open("channelacceptconf", "nut", "node_sim", 1);
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

	// Set the channel accept callback. This implicitly turns on channels for all nodes.
	// This replaces the call to meshlink_set_receive_cb().
	PRINT_TEST_CASE_MSG("Setting Accept callback\n");
	meshlink_set_channel_accept_cb(mesh_handle, channel_accept);
    PRINT_TEST_CASE_MSG("meshlink_set_channel_accept_cb status: %s\n", meshlink_strerror(meshlink_errno));
	assert(meshlink_start(mesh_handle));

    meshlink_channel_t *channel = meshlink_channel_open(mesh_handle, node, CHAT_PORT, NULL, NULL, 0);
    assert(channel!=NULL);


}
