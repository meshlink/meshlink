/*
    run_blackbox_tests.c -- Implementation of Black Box Test Execution for meshlink

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
#include "test_cases.h"
#include "test_cases_open.h"
#include "test_cases_start.h"
#include "test_cases_stop_close.h"
#include "test_cases_send.h"
#include "test_cases_pmtu.h"
#include "test_cases_get_self.h"
#include "test_cases_get_node.h"
#include "test_cases_add_addr.h"
#include "test_cases_get_ex_addr.h"
#include "test_cases_add_ex_addr.h"
#include "test_cases_get_port.h"
#include "test_cases_destroy.h"
#include "test_cases_get_all_nodes.h"
#include "test_cases_get_fingerprint.h"
#include "test_cases_rec_cb.h"
#include "test_cases_sign.h"
#include "test_cases_set_port.h"
#include "test_cases_verify.h"
#include "test_cases_invite.h"
#include "test_cases_export.h"
#include "test_cases_channel_ex.h"
#include "test_cases_channel_get_flags.h"
#include "test_cases_discovery.h"
#include "test_cases_status_cb.h"
#include "test_cases_set_log_cb.h"
#include "test_cases_join.h"


#include "../common/containers.h"
#include "../common/common_handlers.h"

#define CMD_LINE_ARG_MESHLINK_ROOT_PATH 1
#define CMD_LINE_ARG_LXC_PATH 2
#define CMD_LINE_ARG_LXC_BRIDGE_NAME 3
#define CMD_LINE_ARG_ETH_IF_NAME 4
#define CMD_LINE_ARG_CHOOSE_ARCH 5

char *meshlink_root_path = NULL;
char *choose_arch = NULL;

/* State structure for Meta-connections Test Case #1 */
static char *test_meta_conn_1_nodes[] = { "relay", "peer" };
static black_box_state_t test_meta_conn_1_state = {
    /* test_case_name = */ "test_case_meta_conn_01",
    /* node_names = */ test_meta_conn_1_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #2 */
static char *test_meta_conn_2_nodes[] = { "relay", "peer" };
static black_box_state_t test_meta_conn_2_state = {
    /* test_case_name = */ "test_case_meta_conn_02",
    /* node_names = */ test_meta_conn_2_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #3 */
static char *test_meta_conn_3_nodes[] = { "relay", "peer" };
static black_box_state_t test_meta_conn_3_state = {
    /* test_case_name = */ "test_case_meta_conn_03",
    /* node_names = */ test_meta_conn_3_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #4 */
static char *test_meta_conn_4_nodes[] = { "peer" };
static black_box_state_t test_meta_conn_4_state = {
    /* test_case_name = */ "test_case_meta_conn_04",
    /* node_names = */ test_meta_conn_4_nodes,
    /* num_nodes = */ 1,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #5 */
static char *test_meta_conn_5_nodes[] = { "peer" };
static black_box_state_t test_meta_conn_5_state = {
    /* test_case_name = */ "test_case_meta_conn_05",
    /* node_names = */ test_meta_conn_5_nodes,
    /* num_nodes = */ 1,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #1 */
static black_box_state_t test_mesh_open_01_state = {
    /* test_case_name = */ "test_case_mesh_open_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #2 */
static black_box_state_t test_mesh_open_02_state = {
    /* test_case_name = */ "test_case_mesh_open_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #3 */
static black_box_state_t test_mesh_open_03_state = {
    /* test_case_name = */ "test_case_mesh_open_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #4 */
static black_box_state_t test_mesh_open_04_state = {
    /* test_case_name = */ "test_case_mesh_open_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_open Test Case #5 */
static black_box_state_t test_mesh_open_05_state = {
    /* test_case_name = */ "test_case_mesh_open_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_start Test Case #1 */
static black_box_state_t test_mesh_start_01_state = {
    /* test_case_name = */ "test_case_mesh_start_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_start Test Case #2 */
static black_box_state_t test_mesh_start_02_state = {
    /* test_case_name = */ "test_case_mesh_start_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_close Test Case #1 */
static black_box_state_t test_mesh_close_01_state = {
    /* test_case_name = */ "test_case_mesh_close_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_close Test Case #1 */
static black_box_state_t test_mesh_stop_01_state = {
    /* test_case_name = */ "test_case_mesh_stop_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #1 */
static black_box_state_t test_mesh_send_01_state = {
    /* test_case_name = */ "test_case_mesh_send_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #2 */
static black_box_state_t test_mesh_send_02_state = {
    /* test_case_name = */ "test_case_mesh_send_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #3 */
static black_box_state_t test_mesh_send_03_state = {
    /* test_case_name = */ "test_case_mesh_send_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #4 */
static black_box_state_t test_mesh_send_04_state = {
    /* test_case_name = */ "test_case_mesh_send_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #5 */
static black_box_state_t test_mesh_send_05_state = {
    /* test_case_name = */ "test_case_mesh_send_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_send Test Case #6 */
static black_box_state_t test_mesh_send_06_state = {
    /* test_case_name = */ "test_case_mesh_send_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_pmtu Test Case #1 */
static black_box_state_t test_mesh_pmtu_01_state = {
    /* test_case_name = */ "test_case_mesh_pmtu_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_pmtu Test Case #2 */
static black_box_state_t test_mesh_pmtu_02_state = {
    /* test_case_name = */ "test_case_mesh_pmtu_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_pmtu Test Case #3 */
static black_box_state_t test_mesh_pmtu_03_state = {
    /* test_case_name = */ "test_case_mesh_pmtu_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_self Test Case #1 */
static black_box_state_t test_mesh_get_self_01_state = {
    /* test_case_name = */ "test_case_mesh_get_self_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_self Test Case #2 */
static black_box_state_t test_mesh_get_self_02_state = {
    /* test_case_name = */ "test_case_mesh_get_self_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_node Test Case #1 */
static black_box_state_t test_mesh_get_node_01_state = {
    /* test_case_name = */ "test_case_mesh_get_node_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_node Test Case #2 */
static black_box_state_t test_mesh_get_node_02_state = {
    /* test_case_name = */ "test_case_mesh_get_node_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_node Test Case #3 */
static black_box_state_t test_mesh_get_node_03_state = {
    /* test_case_name = */ "test_case_mesh_get_node_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_node Test Case #4 */
static black_box_state_t test_mesh_get_node_04_state = {
    /* test_case_name = */ "test_case_mesh_get_node_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_add_address Test Case #1 */
static black_box_state_t test_mesh_add_address_01_state = {
    /* test_case_name = */ "test_case_mesh_add_address_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_add_address Test Case #2 */
static black_box_state_t test_mesh_add_address_02_state = {
    /* test_case_name = */ "test_case_mesh_add_address_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_add_address Test Case #3 */
static black_box_state_t test_mesh_add_address_03_state = {
    /* test_case_name = */ "test_case_mesh_add_address_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_external_address Test Case #1 */
static black_box_state_t test_mesh_get_address_01_state = {
    /* test_case_name = */ "test_case_mesh_get_address_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_external_address Test Case #2 */
static black_box_state_t test_mesh_get_address_02_state = {
    /* test_case_name = */ "test_case_mesh_get_address_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_add_external_address Test Case #1 */
static black_box_state_t test_mesh_add_ex_address_01_state = {
    /* test_case_name = */ "test_case_mesh_add_ex_address_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_add_external_address Test Case #2 */
static black_box_state_t test_mesh_add_ex_address_02_state = {
    /* test_case_name = */ "test_case_mesh_add_ex_address_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_port Test Case #1 */
static black_box_state_t test_mesh_get_port_01_state = {
    /* test_case_name = */ "test_case_mesh_get_port_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for meshlink_get_port Test Case #2 */
static black_box_state_t test_mesh_get_port_02_state = {
    /* test_case_name = */ "test_case_mesh_get_port_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static char *test_destroy_nodes[] = { "nut" };
static black_box_state_t test_case_meshlink_destroy_01_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_01",
    /* node_names = */ test_destroy_nodes,
    /* num_nodes = */ 1,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_meshlink_destroy_02_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_meshlink_destroy_03_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_meshlink_destroy_04_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_meshlink_destroy_05_state = {
    /* test_case_name = */ "test_case_meshlink_destroy_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};



/* State structure for Meta-connections Test Case #1 */
static black_box_state_t test_case_set_rec_cb_01_state = {
    /* test_case_name = */ "test_case_set_rec_cb_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #1 */
static black_box_state_t test_case_set_rec_cb_02_state = {
    /* test_case_name = */ "test_case_set_rec_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #1 */
static black_box_state_t test_case_set_rec_cb_03_state = {
    /* test_case_name = */ "test_case_set_rec_cb_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #1 */
static black_box_state_t test_case_set_rec_cb_04_state = {
    /* test_case_name = */ "test_case_set_rec_cb_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #1 */
static black_box_state_t test_case_set_rec_cb_05_state = {
    /* test_case_name = */ "test_case_set_rec_cb_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};



/* State structure for get_fingerprint Test Case #1 */
static black_box_state_t test_case_get_fingerprint_cb_01_state = {
    /* test_case_name = */ "test_case_get_fingerprint_cb_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for get_fingerprint Test Case #2 */
static black_box_state_t test_case_get_fingerprint_cb_02_state = {
    /* test_case_name = */ "test_case_get_fingerprint_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for get_fingerprint Test Case #3 */
static black_box_state_t test_case_get_fingerprint_cb_03_state = {
    /* test_case_name = */ "test_case_get_fingerprint_cb_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for test_case_get_fingerprint Test Case #4 */
static black_box_state_t test_case_get_fingerprint_cb_04_state = {
    /* test_case_name = */ "test_case_get_fingerprint_cb_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


static char *test_get_all_nodes_nodes[] = { "peer" };
/* State structure for get_all_nodes Test Case #1 */
static black_box_state_t test_case_get_all_nodes_01_state = {
    /* test_case_name = */ "test_case_get_all_nodes_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for get_all_nodes Test Case #2 */
static black_box_state_t test_case_get_all_nodes_02_state = {
    /* test_case_name = */ "test_case_get_all_nodes_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for get_all_nodes Test Case #3 */
static black_box_state_t test_case_get_all_nodes_03_state = {
    /* test_case_name = */ "test_case_get_all_nodes_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/* State structure for sign API Test Case #1 */
static black_box_state_t test_case_sign_01_state = {
    /* test_case_name = */ "test_case_sign_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #2 */
static black_box_state_t test_case_sign_02_state = {
    /* test_case_name = */ "test_case_sign_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #3 */
static black_box_state_t test_case_sign_03_state = {
    /* test_case_name = */ "test_case_sign_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #4 */
static black_box_state_t test_case_sign_04_state = {
    /* test_case_name = */ "test_case_sign_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #5 */
static black_box_state_t test_case_sign_05_state = {
    /* test_case_name = */ "test_case_sign_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #6 */
static black_box_state_t test_case_sign_06_state = {
    /* test_case_name = */ "test_case_sign_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #7 */
static black_box_state_t test_case_sign_07_state = {
    /* test_case_name = */ "test_case_sign_07",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};



/* State structure for verify API Test Case #1 */
static black_box_state_t test_case_verify_01_state = {
    /* test_case_name = */ "test_case_verify_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #2 */
static black_box_state_t test_case_verify_02_state = {
    /* test_case_name = */ "test_case_verify_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #3 */
static black_box_state_t test_case_verify_03_state = {
    /* test_case_name = */ "test_case_verify_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #4 */
static black_box_state_t test_case_verify_04_state = {
    /* test_case_name = */ "test_case_verify_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #5 */
static black_box_state_t test_case_verify_05_state = {
    /* test_case_name = */ "test_case_verify_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #6 */
static black_box_state_t test_case_verify_06_state = {
    /* test_case_name = */ "test_case_verify_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for verify API Test Case #7 */
static black_box_state_t test_case_verify_07_state = {
    /* test_case_name = */ "test_case_verify_07",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for set port API Test Case #1 */
static black_box_state_t test_case_set_port_01_state = {
    /* test_case_name = */ "test_case_set_port_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for set port API Test Case #2 */
static black_box_state_t test_case_set_port_02_state = {
    /* test_case_name = */ "test_case_set_port_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for set port API Test Case #3 */
static black_box_state_t test_case_set_port_03_state = {
    /* test_case_name = */ "test_case_set_port_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for set port API Test Case #4 */
static black_box_state_t test_case_set_port_04_state = {
    /* test_case_name = */ "test_case_set_port_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for set port API Test Case #5 */
static black_box_state_t test_case_set_port_05_state = {
    /* test_case_name = */ "test_case_set_port_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/* State structure for invite API Test Case #1 */
static black_box_state_t test_case_invite_01_state = {
    /* test_case_name = */ "test_case_invite_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for invite API Test Case #2 */
static black_box_state_t test_case_invite_02_state = {
    /* test_case_name = */ "test_case_invite_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for invite API Test Case #3 */
static black_box_state_t test_case_invite_03_state = {
    /* test_case_name = */ "test_case_invite_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for invite API Test Case #4 */
static black_box_state_t test_case_invite_04_state = {
    /* test_case_name = */ "test_case_invite_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for invite API Test Case #5 */
static black_box_state_t test_case_invite_05_state = {
    /* test_case_name = */ "test_case_invite_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for invite API Test Case #6 */
static black_box_state_t test_case_invite_06_state = {
    /* test_case_name = */ "test_case_invite_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


/* State structure for sign API Test Case #1 */
static black_box_state_t test_case_export_01_state = {
    /* test_case_name = */ "test_case_export_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #2 */
static black_box_state_t test_case_export_02_state = {
    /* test_case_name = */ "test_case_export_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #3 */
static black_box_state_t test_case_export_03_state = {
    /* test_case_name = */ "test_case_export_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for sign API Test Case #4 */
static black_box_state_t test_case_export_04_state = {
    /* test_case_name = */ "test_case_export_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};


static black_box_state_t test_case_channel_ex_01_state = {
    /* test_case_name = */ "test_case_channel_ex_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_02_state = {
    /* test_case_name = */ "test_case_channel_ex_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_03_state = {
    /* test_case_name = */ "test_case_channel_ex_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_04_state = {
    /* test_case_name = */ "test_case_channel_ex_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_05_state = {
    /* test_case_name = */ "test_case_channel_ex_05",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_06_state = {
    /* test_case_name = */ "test_case_channel_ex_06",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_ex_07_state = {
    /* test_case_name = */ "test_case_channel_ex_07",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_get_flags_01_state = {
    /* test_case_name = */ "test_case_channel_get_flags_01",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_get_flags_02_state = {
    /* test_case_name = */ "test_case_channel_get_flags_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_get_flags_03_state = {
    /* test_case_name = */ "test_case_channel_get_flags_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_channel_get_flags_04_state = {
    /* test_case_name = */ "test_case_channel_get_flags_04",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};




/* State structure for Meta-connections Test Case #1 */
static char *test_discovery_1_nodes[] = { "relay", "peer" };
static black_box_state_t test_case_discovery_01_state = {
    /* test_case_name = */ "test_case_discovery_01",
    /* node_names = */ test_discovery_1_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #2 */
static char *test_discovery_2_nodes[] = { "relay", "peer" };
static black_box_state_t test_case_discovery_02_state = {
    /* test_case_name = */ "test_case_discovery_02",
    /* node_names = */ test_discovery_2_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for Meta-connections Test Case #3 */
static black_box_state_t test_case_discovery_03_state = {
    /* test_case_name = */ "test_case_meta_conn_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for status callback Test Case #1 */
static char *test_stat_1_nodes[] = { "relay", "peer" };
static black_box_state_t test_case_set_status_cb_01_state = {
    /* test_case_name = */ "test_case_set_status_cb_01",
    /* node_names = */ test_stat_1_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for status callback Test Case #2 */
static black_box_state_t test_case_set_status_cb_02_state = {
    /* test_case_name = */ "test_case_set_status_cb_02",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for status callback Test Case #3 */
static char *test_stat_3_nodes[] = { "relay", "peer" };
static black_box_state_t test_case_set_status_cb_03_state = {
    /* test_case_name = */ "test_case_set_status_cb_03",
    /* node_names = */ test_stat_3_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for log callback Test Case #1 */
static char *test_log_1_nodes[] = { "relay", "peer" };
static black_box_state_t test_case_set_log_cb_01_state = {
    /* test_case_name = */ "test_case_set_log_cb_01",
    /* node_names = */ test_log_1_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

/* State structure for log callback Test Case #1 */
static char *test_log_2_nodes[] = { "relay", "peer" };
static black_box_state_t test_case_set_log_cb_02_state = {
    /* test_case_name = */ "test_case_set_log_cb_02",
    /* node_names = */ test_log_2_nodes,
    /* num_nodes = */ 2,
    /* test_result (defaulted to) = */ false
};

static black_box_state_t test_case_set_log_cb_03_state = {
    /* test_case_name = */ "test_case_set_log_cb_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};




/* State structure for join Test Case #1 */
static char *test_join_1_nodes[] = { "relay" };
static black_box_state_t test_case_join_01_state = {
    /* test_case_name = */ "test_case_join_01",
    /* node_names = */ test_join_1_nodes,
    /* num_nodes = */ 1,
    /* test_result (defaulted to) = */ false
};

/* State structure for join Test Case #1 */
static char *test_join_2_nodes[] = { "relay" };
static black_box_state_t test_case_join_02_state = {
    /* test_case_name = */ "test_case_join_02",
    /* node_names = */ test_join_2_nodes,
    /* num_nodes = */ 1,
    /* test_result (defaulted to) = */ false
};

/* State structure for join Test Case #1 */
static black_box_state_t test_case_join_03_state = {
    /* test_case_name = */ "test_case_join_03",
    /* node_names = */ NULL,
    /* num_nodes = */ 0,
    /* test_result (defaulted to) = */ false
};

/* State structure for join Test Case #1 */
static char *test_join_4_nodes[] = { "relay" };
static black_box_state_t test_case_join_04_state = {
    /* test_case_name = */ "test_case_join_04",
    /* node_names = */ test_join_4_nodes,
    /* num_nodes = */ 1,
    /* test_result (defaulted to) = */ false
};

/* State structure for join Test Case #1 */
static char *test_join_5_nodes[] = { "relay" };
static black_box_state_t test_case_join_05_state = {
    /* test_case_name = */ "test_case_join_05",
    /* node_names = */ test_join_5_nodes,
    /* num_nodes = */ 1,
    /* test_result (defaulted to) = */ false
};





int black_box_group0_setup(void **state) {
    char *nodes[] = { "peer", "relay" };
    int num_nodes = sizeof(nodes) / sizeof(nodes[0]);

    printf("Creating Containers\n");
    destroy_containers();
    create_containers(nodes, num_nodes);

    return 0;
}

int black_box_group0_teardown(void **state) {
    printf("Destroying Containers\n");
    destroy_containers();

    return 0;
}

int main(int argc, char *argv[]) {
    const struct CMUnitTest blackbox_group0_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_01, setup_test, teardown_test,
            (void *)&test_meta_conn_1_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_02, setup_test, teardown_test,
            (void *)&test_meta_conn_2_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_03, setup_test, teardown_test,
            (void *)&test_meta_conn_3_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_04, setup_test, teardown_test,
            (void *)&test_meta_conn_4_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meta_conn_05, setup_test, teardown_test,
            (void *)&test_meta_conn_5_state)
    };
		const struct CMUnitTest blackbox_open_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_01, NULL, NULL,
            (void *)&test_mesh_open_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_02, NULL, NULL,
            (void *)&test_mesh_open_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_03, NULL, NULL,
            (void *)&test_mesh_open_03_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_04, NULL, NULL,
            (void *)&test_mesh_open_04_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_open_05, NULL, NULL,
            (void *)&test_mesh_open_05_state)

		};
		const struct CMUnitTest blackbox_start_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_start_01, NULL, NULL,
            (void *)&test_mesh_start_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_start_02, NULL, NULL,
            (void *)&test_mesh_start_02_state)

		};
		const struct CMUnitTest blackbox_stop_close_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_close_01, NULL, NULL,
            (void *)&test_mesh_close_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_stop_01, NULL, NULL,
            (void *)&test_mesh_stop_01_state)
		};
		const struct CMUnitTest blackbox_send_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_01, NULL, NULL,
            (void *)&test_mesh_send_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_02, NULL, NULL,
            (void *)&test_mesh_send_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_03, NULL, NULL,
            (void *)&test_mesh_send_03_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_04, NULL, NULL,
            (void *)&test_mesh_send_04_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_05, NULL, NULL,
            (void *)&test_mesh_send_05_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_send_06, NULL, NULL,
            (void *)&test_mesh_send_06_state)
		};
		const struct CMUnitTest blackbox_pmtu_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_pmtu_01, NULL, NULL,
            (void *)&test_mesh_pmtu_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_pmtu_02, NULL, NULL,
            (void *)&test_mesh_pmtu_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_pmtu_03, NULL, NULL,
            (void *)&test_mesh_pmtu_03_state)
		};
		const struct CMUnitTest blackbox_get_self_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_self_01, NULL, NULL,
            (void *)&test_mesh_get_self_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_self_02, NULL, NULL,
            (void *)&test_mesh_get_self_02_state)
		};
		const struct CMUnitTest blackbox_get_node_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_01, NULL, NULL,
            (void *)&test_mesh_get_node_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_02, NULL, NULL,
            (void *)&test_mesh_get_node_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_03, NULL, NULL,
            (void *)&test_mesh_get_node_03_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_node_04, NULL, NULL,
            (void *)&test_mesh_get_node_04_state)
		};
		const struct CMUnitTest blackbox_add_addr_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_add_address_01, NULL, NULL,
            (void *)&test_mesh_add_address_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_add_address_02, NULL, NULL,
            (void *)&test_mesh_add_address_02_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_add_address_03, NULL, NULL,
            (void *)&test_mesh_add_address_03_state)
		};
		const struct CMUnitTest blackbox_get_ex_addr_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_address_01, NULL, NULL,
            (void *)&test_mesh_get_address_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_address_02, NULL, NULL,
            (void *)&test_mesh_get_address_02_state)
		};
		const struct CMUnitTest blackbox_add_ex_addr_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_add_ex_address_01, NULL, NULL,
            (void *)&test_mesh_add_ex_address_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_add_ex_address_02, NULL, NULL,
            (void *)&test_mesh_add_ex_address_02_state)
		};
		const struct CMUnitTest blackbox_get_port_tests[] = {
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_port_01, NULL, NULL,
            (void *)&test_mesh_get_port_01_state),
				cmocka_unit_test_prestate_setup_teardown(test_case_mesh_get_port_02, NULL, NULL,
            (void *)&test_mesh_get_port_02_state)
		};
    const struct CMUnitTest blackbox_destroy_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_01, NULL, NULL,
            (void *)&test_case_meshlink_destroy_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_02, NULL, NULL,
            (void *)&test_case_meshlink_destroy_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_03, NULL, NULL,
            (void *)&test_case_meshlink_destroy_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_04, NULL, NULL,
            (void *)&test_case_meshlink_destroy_04_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_destroy_05, NULL, NULL,
            (void *)&test_case_meshlink_destroy_05_state)
    };
    int num_tests_destroy = sizeof(blackbox_destroy_tests) / sizeof(blackbox_destroy_tests[0]);


    const struct CMUnitTest blackbox_receive_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_01, NULL, NULL,
            (void *)&test_case_set_rec_cb_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_02, NULL, NULL,
            (void *)&test_case_set_rec_cb_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_03, NULL, NULL,
            (void *)&test_case_set_rec_cb_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_rec_cb_04, NULL, NULL,
            (void *)&test_case_set_rec_cb_04_state)
    };
    int num_tests_receive = sizeof(blackbox_receive_tests) / sizeof(blackbox_receive_tests[0]);


    const struct CMUnitTest blackbox_get_fingerprint_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_get_fingerprint_cb_01, NULL, NULL,
            (void *)&test_case_get_fingerprint_cb_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_get_fingerprint_cb_02, NULL, NULL,
            (void *)&test_case_get_fingerprint_cb_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_get_fingerprint_cb_03, NULL, NULL,
            (void *)&test_case_get_fingerprint_cb_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_get_fingerprint_cb_04, NULL, NULL,
            (void *)&test_case_get_fingerprint_cb_04_state)
    };
    int num_tests_get_fp = sizeof(blackbox_get_fingerprint_tests) / sizeof(blackbox_get_fingerprint_tests[0]);


    const struct CMUnitTest blackbox_get_all_nodes[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_get_all_nodes_01, NULL, NULL,
            (void *)&test_case_get_all_nodes_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_get_all_nodes_02, NULL, NULL,
            (void *)&test_case_get_all_nodes_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_get_all_nodes_03, NULL, NULL,
            (void *)&test_case_get_all_nodes_03_state)};
    int num_tests_get_all = sizeof(blackbox_get_all_nodes) / sizeof(blackbox_get_all_nodes[0]);

    const struct CMUnitTest blackbox_sign_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_sign_01, NULL, NULL,
            (void *)&test_case_sign_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_sign_02, NULL, NULL,
            (void *)&test_case_sign_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_sign_03, NULL, NULL,
            (void *)&test_case_sign_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_sign_04, NULL, NULL,
            (void *)&test_case_sign_04_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_sign_05, NULL, NULL,
            (void *)&test_case_sign_05_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_sign_06, NULL, NULL,
            (void *)&test_case_sign_06_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_sign_07, NULL, NULL,
            (void *)&test_case_sign_07_state)
    };
    int num_tests_sign = sizeof(blackbox_sign_tests) / sizeof(blackbox_sign_tests[0]);

    const struct CMUnitTest blackbox_verify_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_verify_01, NULL, NULL,
            (void *)&test_case_verify_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_verify_02, NULL, NULL,
            (void *)&test_case_verify_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_verify_03, NULL, NULL,
            (void *)&test_case_verify_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_verify_04, NULL, NULL,
            (void *)&test_case_verify_04_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_verify_05, NULL, NULL,
            (void *)&test_case_verify_05_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_verify_06, NULL, NULL,
            (void *)&test_case_verify_06_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_verify_07, NULL, NULL,
            (void *)&test_case_verify_07_state)
    };
    int num_tests_verify = sizeof(blackbox_verify_tests) / sizeof(blackbox_verify_tests[0]);


    const struct CMUnitTest blackbox_set_port_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_set_port_01, NULL, NULL,
            (void *)&test_case_set_port_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_port_02, NULL, NULL,
            (void *)&test_case_set_port_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_port_03, NULL, NULL,
            (void *)&test_case_set_port_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_port_04, NULL, NULL,
            (void *)&test_case_set_port_04_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_port_05, NULL, NULL,
            (void *)&test_case_set_port_05_state)
    };
    int num_tests_set_port = sizeof(blackbox_set_port_tests) / sizeof(blackbox_set_port_tests[0]);


    const struct CMUnitTest blackbox_invite_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_01, NULL, NULL,
            (void *)&test_case_invite_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_02, NULL, NULL,
            (void *)&test_case_invite_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_03, NULL, NULL,
            (void *)&test_case_invite_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_04, NULL, NULL,
            (void *)&test_case_invite_04_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_05, NULL, NULL,
            (void *)&test_case_invite_05_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_invite_06, NULL, NULL,
            (void *)&test_case_invite_06_state)
    };
    int num_tests_invite = sizeof(blackbox_invite_tests) / sizeof(blackbox_invite_tests[0]);

    const struct CMUnitTest blackbox_export_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_export_01, NULL, NULL,
            (void *)&test_case_export_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_export_02, NULL, NULL,
            (void *)&test_case_export_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_export_03, NULL, NULL,
            (void *)&test_case_export_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_export_04, NULL, NULL,
            (void *)&test_case_export_04_state)
    };
    int num_tests_export = sizeof(blackbox_export_tests) / sizeof(blackbox_export_tests[0]);

    const struct CMUnitTest blackbox_channel_ex_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_01, NULL, NULL,
            (void *)&test_case_channel_ex_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_02, NULL, NULL,
            (void *)&test_case_channel_ex_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_03, NULL, NULL,
            (void *)&test_case_channel_ex_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_04, NULL, NULL,
            (void *)&test_case_channel_ex_04_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_05, NULL, NULL,
            (void *)&test_case_channel_ex_05_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_06, NULL, NULL,
            (void *)&test_case_channel_ex_06_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_ex_07, NULL, NULL,
            (void *)&test_case_channel_ex_07_state)
    };

    const struct CMUnitTest blackbox_channel_get_flags_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_01, NULL, NULL,
            (void *)&test_case_channel_get_flags_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_02, NULL, NULL,
            (void *)&test_case_channel_get_flags_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_03, NULL, NULL,
            (void *)&test_case_channel_get_flags_03_state),
       cmocka_unit_test_prestate_setup_teardown(test_case_channel_get_flags_04, NULL, NULL,
           (void *)&test_case_channel_get_flags_04_state)
    };


    const struct CMUnitTest blackbox_discovery_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_discovery_01, setup_test, teardown_test,
            (void *)&test_case_discovery_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_discovery_02, setup_test, teardown_test,
            (void *)&test_case_discovery_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_discovery_03, setup_test, teardown_test,
           (void *)&test_case_discovery_03_state)
    };

    const struct CMUnitTest blackbox_status_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_set_status_cb_01, setup_test, teardown_test,
            (void *)&test_case_set_status_cb_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_status_cb_02, NULL, NULL,
            (void *)&test_case_set_status_cb_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_status_cb_03, setup_test, teardown_test,
            (void *)&test_case_set_status_cb_03_state)
    };
    int num_tests_status = sizeof(blackbox_status_tests) / sizeof(blackbox_status_tests[0]);

    const struct CMUnitTest blackbox_log_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_set_log_cb_01, setup_test, teardown_test,
            (void *)&test_case_set_log_cb_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_log_cb_02, setup_test, teardown_test,
            (void *)&test_case_set_log_cb_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_set_log_cb_03, NULL, NULL,
            (void *)&test_case_set_log_cb_03_state)
    };
    int num_tests_log = sizeof(blackbox_log_tests) / sizeof(blackbox_log_tests[0]);


    const struct CMUnitTest blackbox_join_tests[] = {
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_01, setup_test, teardown_test,
            (void *)&test_case_join_01_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_02, setup_test, teardown_test,
            (void *)&test_case_join_02_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_03, NULL, NULL,
            (void *)&test_case_join_03_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_04, setup_test, teardown_test,
            (void *)&test_case_join_04_state),
        cmocka_unit_test_prestate_setup_teardown(test_case_meshlink_join_05, setup_test, teardown_test,
            (void *)&test_case_join_05_state)
    };
    int num_tests_join = sizeof(blackbox_join_tests) / sizeof(blackbox_join_tests[0]);


    int num0_tests = sizeof(blackbox_group0_tests) / sizeof(blackbox_group0_tests[0]);
    int num1_tests = sizeof(blackbox_open_tests) / sizeof(blackbox_open_tests[0]);
    int num2_tests = sizeof(blackbox_start_tests) / sizeof(blackbox_start_tests[0]);
    int num3_tests = sizeof(blackbox_stop_close_tests) / sizeof(blackbox_stop_close_tests[0]);
    int num4_tests = sizeof(blackbox_send_tests) / sizeof(blackbox_send_tests[0]);
    int num5_tests = sizeof(blackbox_pmtu_tests) / sizeof(blackbox_pmtu_tests[0]);
    int num6_tests = sizeof(blackbox_get_self_tests) / sizeof(blackbox_get_self_tests[0]);
    int num7_tests = sizeof(blackbox_get_node_tests) / sizeof(blackbox_get_node_tests[0]);
    int num8_tests = sizeof(blackbox_add_addr_tests) / sizeof(blackbox_add_addr_tests[0]);
    int num9_tests = sizeof(blackbox_get_ex_addr_tests) / sizeof(blackbox_get_ex_addr_tests[0]);
    int num10_tests = sizeof(blackbox_add_ex_addr_tests) / sizeof(blackbox_add_ex_addr_tests[0]);
    int num11_tests = sizeof(blackbox_get_port_tests) / sizeof(blackbox_get_port_tests[0]);
    int num_channel_ex_tests = sizeof(blackbox_channel_ex_tests) / sizeof(blackbox_channel_ex_tests[0]);

		int num_tests = num0_tests + num1_tests + num2_tests + num3_tests + num4_tests + num5_tests + num6_tests + num7_tests + num8_tests + num9_tests + num10_tests + num11_tests + num_tests_destroy + num_tests_receive + num_tests_get_fp + num_tests_get_all + num_tests_sign + num_tests_verify + num_tests_set_port + num_tests_invite + num_tests_export;

        int failed_tests = 0, group0failed = 0, group1failed = 0, group2failed = 0, group3failed = 0, group4failed = 0, group5failed = 0, group6failed = 0, group7failed = 0, group8failed = 0, group9failed = 0, group10failed = 0, group11failed = 0, group12failed = 0, group13failed = 0, group14failed = 0, group15failed = 0, group16failed = 0, group17failed = 0, group18failed = 0, group19failed = 0, group20failed = 0;

        int groupchannelexfailed = 0, channelflagsfailed = 0, discfailed = 0, joinfailed = 0, logfailed = 0;
    /* Set configuration */
    assert(argc >= (CMD_LINE_ARG_CHOOSE_ARCH + 1));
    meshlink_root_path = argv[CMD_LINE_ARG_MESHLINK_ROOT_PATH];
    lxc_path = argv[CMD_LINE_ARG_LXC_PATH];
    lxc_bridge = argv[CMD_LINE_ARG_LXC_BRIDGE_NAME];
    eth_if_name = argv[CMD_LINE_ARG_ETH_IF_NAME];
		choose_arch = argv[CMD_LINE_ARG_CHOOSE_ARCH];

//		group0failed = cmocka_run_group_tests(blackbox_group0_tests, black_box_group0_setup, black_box_group0_teardown);
/*		group1failed = cmocka_run_group_tests(blackbox_open_tests, NULL, NULL);
		group2failed = cmocka_run_group_tests(blackbox_start_tests, NULL, NULL);
		group3failed = cmocka_run_group_tests(blackbox_stop_close_tests, NULL, NULL);
		group4failed = cmocka_run_group_tests(blackbox_send_tests, NULL, NULL);
		group5failed = cmocka_run_group_tests(blackbox_pmtu_tests, NULL, NULL);
		group6failed = cmocka_run_group_tests(blackbox_get_self_tests, NULL, NULL);
		group7failed = cmocka_run_group_tests(blackbox_get_node_tests, NULL, NULL);
		group8failed = cmocka_run_group_tests(blackbox_add_addr_tests, NULL, NULL);
		group9failed = cmocka_run_group_tests(blackbox_get_ex_addr_tests, NULL, NULL);
		group10failed = cmocka_run_group_tests(blackbox_add_ex_addr_tests, NULL, NULL);
		group11failed = cmocka_run_group_tests(blackbox_get_port_tests, NULL, NULL);
		group12failed = cmocka_run_group_tests(blackbox_destroy_tests, NULL, NULL);
		group13failed = cmocka_run_group_tests(blackbox_receive_tests, NULL, NULL);
		group14failed = cmocka_run_group_tests(blackbox_get_fingerprint_tests, NULL, NULL);
		group15failed = cmocka_run_group_tests(blackbox_get_all_nodes, NULL, NULL);
		group16failed = cmocka_run_group_tests(blackbox_sign_tests, NULL, NULL);
		group17failed = cmocka_run_group_tests(blackbox_verify_tests, NULL, NULL);
		group18failed = cmocka_run_group_tests(blackbox_set_port_tests, NULL, NULL);
		group19failed = cmocka_run_group_tests(blackbox_invite_tests, NULL, NULL);
		group20failed = cmocka_run_group_tests(blackbox_export_tests, NULL, NULL);
//		groupchannelexfailed = cmocka_run_group_tests(blackbox_channel_ex_tests, NULL, NULL);

//		channelflagsfailed = cmocka_run_group_tests(blackbox_channel_get_flags_tests, NULL, NULL);
       int statfailed = cmocka_run_group_tests(blackbox_status_tests, black_box_group0_setup, black_box_group0_teardown);

		discfailed = cmocka_run_group_tests(blackbox_discovery_tests, black_box_group0_setup, black_box_group0_teardown);*/

//    logfailed = cmocka_run_group_tests(blackbox_log_tests, black_box_group0_setup, black_box_group0_teardown);

    joinfailed = cmocka_run_group_tests(blackbox_join_tests, black_box_group0_setup, black_box_group0_teardown);

		failed_tests = logfailed + group1failed + group2failed + group3failed + group4failed + group5failed + group6failed + group7failed + group8failed + group9failed + group10failed + group11failed + group12failed + group13failed +group14failed + group15failed + group16failed + group17failed + group18failed + group19failed + group20failed;


    printf("[ PASSED ] %d test(s).\n", num_tests - failed_tests);
    printf("[ FAILED ] %d test(s).\n", failed_tests);

    return failed_tests;
}

