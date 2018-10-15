/*
    test_step.c -- Handlers for executing test steps during node simulation
    Copyright (C) 2017  Guus Sliepen <guus@meshlink.io>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
ŝ
    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "../../../src/meshlink.h"
#include "test_step.h"
#include "common_handlers.h"

/* Modify this to change the logging level of Meshlink */
#define TEST_MESHLINK_LOG_LEVEL MESHLINK_INFO

meshlink_handle_t *mesh_handle = NULL;
bool mesh_started = false;
char *eth_if_name = NULL;

meshlink_handle_t *execute_open(char *node_name, char *dev_class) {
    /* Set up logging for Meshlink */
    meshlink_set_log_cb(NULL, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);

    /* Create meshlink instance */
    mesh_handle = meshlink_open("testconf", node_name, "node_sim", atoi(dev_class));
    PRINT_TEST_CASE_MSG("meshlink_open status: %s\n", meshlink_strerror(meshlink_errno));
    assert(mesh_handle);

    /* Set up logging for Meshlink with the newly acquired Mesh Handle */
    meshlink_set_log_cb(mesh_handle, TEST_MESHLINK_LOG_LEVEL, meshlink_callback_logger);
    /* Set up callback for node status (reachable / unreachable) */
    meshlink_set_node_status_cb(mesh_handle, meshlink_callback_node_status);

    return mesh_handle;
}

char *execute_invite(char *invitee) {
    char *invite_url = meshlink_invite(mesh_handle, invitee);

    PRINT_TEST_CASE_MSG("meshlink_invite status: %s\n", meshlink_strerror(meshlink_errno));
    assert(invite_url);

    return invite_url;
}

void execute_join(char *invite_url) {
    bool join_status;

    /* The inviting node may take a moment to open its listening port
        This sleep() prevents meshlink_join() from failing when the listening port is not open */
    /* TO DO: Replace this with code that actually checks for the port being open, if possible */
    PRINT_TEST_CASE_MSG("Sleeping 1 sec to allow inviting node to start listening...\n");
    sleep(1);

    PRINT_TEST_CASE_MSG("About to join with mesh_handle = %p, invite_url = %s\n", mesh_handle, invite_url);
    join_status = meshlink_join(mesh_handle, invite_url);
    PRINT_TEST_CASE_MSG("meshlink_join status: %s\n", meshlink_strerror(meshlink_errno));
    assert(join_status);

    return;
}

void execute_start(void) {
    bool start_init_status = meshlink_start(mesh_handle);

    PRINT_TEST_CASE_MSG("meshlink_start status: %s\n", meshlink_strerror(meshlink_errno));
    assert(start_init_status);
    mesh_started = true;

    return;
}

void execute_stop(void) {
    assert(mesh_handle);
    meshlink_stop(mesh_handle);
    mesh_started = false;

    return;
}

void execute_close(void) {
    assert(mesh_handle);
    meshlink_close(mesh_handle);

    return;
}

void execute_change_ip(void) {
    char *eth_if_ip;
    int last_byte;
    char new_ip[20], gateway_ip[20];
    char *last_dot_in_ip;
    char *eth_if_netmask;
    char route_chg_command[200];
    int route_chg_status;

    /* Get existing IP Address of Ethernet Bridge Interface */
    assert(eth_if_ip = get_ip(eth_if_name));

    /* Set new IP Address by replacing the last byte with last byte + 1 */
    strncpy(new_ip, eth_if_ip, sizeof(new_ip));
    assert(last_dot_in_ip = strrchr(new_ip, '.'));
    last_byte = atoi(last_dot_in_ip + 1);
    assert(snprintf(last_dot_in_ip + 1, 4, "%d", (last_byte > 253) ? 2 : (last_byte + 1)) >= 0);

    /* TO DO: Check for IP conflicts with other interfaces and existing Containers */
    /* Bring the network interface down before making changes */
    stop_nw_intf(eth_if_name);
    /* Save the netmask first, then restore it after setting the new IP Address */
    assert(eth_if_netmask = get_netmask(eth_if_name));
    set_ip(eth_if_name, new_ip);
    set_netmask(eth_if_name, eth_if_netmask);
    /* Bring the network interface back up again to apply changes */
    start_nw_intf(eth_if_name);

    /* Get Gateway's IP Address, by replacing the last byte with 1 in the current IP Address */
    /* TO DO: Obtain the actual Gateway IP Address */
    strncpy(gateway_ip, eth_if_ip, sizeof(gateway_ip));
    assert(last_dot_in_ip = strrchr(gateway_ip, '.'));
    assert(snprintf(last_dot_in_ip + 1, 4, "%d", 1) >= 0);

    /* Add the default route back again, which would have been deleted when the
        network interface was brought down */
    /* TO DO: Perform this action using ioctl with SIOCADDRT */
    /*assert(snprintf(route_chg_command, sizeof(route_chg_command), "route add default gw %s",
      gateway_ip) >= 0);
     route_chg_status = system(route_chg_command);
     PRINT_TEST_CASE_MSG("Default Route Add status = %d\n", route_chg_status);
     assert(route_chg_status == 0); */
		// Not necessary for ubuntu versions of 16.04 and 18.04

    PRINT_TEST_CASE_MSG("Node '%s' IP Address changed to %s\n", NUT_NODE_NAME, new_ip);

    free(eth_if_ip);
    free(eth_if_netmask);

    return;
}

