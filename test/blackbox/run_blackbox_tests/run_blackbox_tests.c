/*
    run_blackbox_tests.c -- Implementation of Black Box Test Execution for meshlink

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
#include "test_cases_blacklist.h"
#include "test_cases_default_blacklist.h"
#include "test_cases_whitelist.h"
#include "test_cases_channel_open.h"
#include "test_cases_channel_close.h"
#include "test_cases_channel_send.h"
#include "test_cases_channel_shutdown.h"

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
#include "test_cases_status_cb.h"
#include "test_cases_set_log_cb.h"
#include "test_cases_join.h"
#include "test_cases_import.h"
#include "test_cases_channel_set_accept_cb.h"
#include "test_cases_channel_set_poll_cb.h"
#include "test_cases_channel_set_receive_cb.h"
#include "test_cases_hint_address.h"
#include "test_optimal_pmtu.h"

#include "test_cases_channel_conn.h"
#include "test_cases_get_all_nodes_by_dev_class.h"
#include "test_cases_submesh01.h"
#include "test_cases_submesh02.h"
#include "test_cases_submesh03.h"
#include "test_cases_submesh04.h"

#include "test_cases_random_port_bindings01.h"
#include "test_cases_random_port_bindings02.h"

#include "../common/containers.h"
#include "../common/common_handlers.h"

char *meshlink_root_path = NULL;
char *choose_arch = NULL;
int total_tests;

int main(int argc, char *argv[]) {
	/* Set configuration */
	assert(argc > 5);
	meshlink_root_path = argv[1];
	lxc_path = argv[2];
	lxc_bridge = argv[3];
	eth_if_name = argv[4];
	choose_arch = argv[5];

	int failed_tests = 0;

	/*
	failed_tests += test_meta_conn();
	failed_tests += test_meshlink_set_status_cb();
	failed_tests += test_meshlink_join();
	failed_tests += test_meshlink_set_channel_poll_cb();
	failed_tests += test_meshlink_channel_open_ex();
	failed_tests += test_meshlink_channel_get_flags();
	failed_tests += test_meshlink_set_channel_accept_cb();
	failed_tests += test_meshlink_destroy();
	failed_tests += test_meshlink_export();
	failed_tests += test_meshlink_get_fingerprint();
	failed_tests += test_meshlink_get_all_nodes();
	failed_tests += test_meshlink_get_all_node_by_device_class();
	failed_tests += test_meshlink_set_port();
	failed_tests += test_meshlink_sign();
	failed_tests += test_meshlink_verify();
	failed_tests += test_meshlink_import();
	failed_tests += test_meshlink_invite();
	failed_tests += test_meshlink_set_receive_cb();
	failed_tests += test_meshlink_set_log_cb();
	failed_tests += test_meshlink_set_channel_receive_cb();
	failed_tests += test_meshlink_hint_address();

	failed_tests += test_meshlink_open();
	failed_tests += test_meshlink_start();
	failed_tests += test_meshlink_stop_close();
	failed_tests += test_meshlink_send();
	failed_tests += test_meshlink_channel_send();
	failed_tests += test_meshlink_channel_shutdown();
	failed_tests += test_meshlink_pmtu();
	failed_tests += test_meshlink_get_self();
	failed_tests += test_meshlink_get_node();
	failed_tests += test_meshlink_add_address();
	failed_tests += test_meshlink_get_external_address();
	failed_tests += test_meshlink_add_external_address();
	failed_tests += test_meshlink_get_port();
	failed_tests += test_meshlink_blacklist();
	failed_tests += test_meshlink_whitelist();
	failed_tests += test_meshlink_default_blacklist();
	failed_tests += test_meshlink_channel_open();
	failed_tests += test_meshlink_channel_close();

	failed_tests += test_meshlink_channel_conn();
	failed_tests += test_optimal_pmtu();
	*/
	failed_tests += test_cases_submesh01();
	failed_tests += test_cases_submesh02();
	failed_tests += test_cases_submesh03();
	failed_tests += test_cases_submesh04();

	failed_tests += test_optimal_pmtu();

	failed_tests += test_meshlink_random_port_bindings01();
	failed_tests += test_meshlink_random_port_bindings02();

	printf("[ PASSED ] %d test(s).\n", total_tests - failed_tests);
	printf("[ FAILED ] %d test(s).\n", failed_tests);

	return failed_tests;
}
