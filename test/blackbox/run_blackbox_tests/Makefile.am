check_PROGRAMS = run_blackbox_tests

run_blackbox_tests_SOURCES = \
	run_blackbox_tests.c \
	test_cases.c execute_tests.c \
	../common/mesh_event_handler.c \
	../common/containers.c \
	../common/tcpdump.c \
	../common/common_handlers.c \
	../common/test_step.c \
	../common/network_namespace_framework.c \
	../../utils.c \
	test_cases_destroy.c \
	test_cases_export.c \
	test_cases_get_all_nodes.c \
	test_cases_get_fingerprint.c \
	test_cases_invite.c \
	test_cases_rec_cb.c \
	test_cases_set_port.c \
	test_cases_sign.c \
	test_cases_verify.c \
	test_cases_channel_ex.c \
	test_cases_channel_get_flags.c \
	test_cases_status_cb.c \
	test_cases_set_log_cb.c \
	test_cases_join.c \
	test_cases_import.c \
	test_cases_channel_set_accept_cb.c \
	test_cases_channel_set_poll_cb.c \
	test_cases_hint_address.c \
	test_cases_channel_set_receive_cb.c \
	test_cases_open.c \
	test_cases_start.c \
	test_cases_stop_close.c \
	test_cases_pmtu.c \
	test_cases_get_self.c \
	test_cases_send.c \
	test_cases_get_node.c \
	test_cases_add_addr.c \
	test_cases_get_ex_addr.c \
	test_cases_add_ex_addr.c \
	test_cases_get_port.c \
	test_cases_blacklist.c \
	test_cases_whitelist.c \
	test_cases_default_blacklist.c \
	test_cases_channel_open.c \
	test_cases_channel_close.c \
	test_cases_channel_send.c \
	test_cases_channel_shutdown.c \
	test_cases_channel_conn.c \
	test_cases_get_all_nodes_by_dev_class.c \
	../test_case_optimal_pmtu_01/node_sim_nut.c \
	../test_case_optimal_pmtu_01/node_sim_relay.c \
	../test_case_optimal_pmtu_01/node_sim_peer.c \
	test_optimal_pmtu.c \
	../test_case_channel_blacklist_01/node_sim_nut_01.c \
	../test_case_channel_blacklist_01/node_sim_peer_01.c \
	../test_case_channel_blacklist_01/node_sim_relay_01.c \
	test_cases_channel_blacklist.c \
	test_cases_submesh01.c \
	test_cases_submesh02.c \
	test_cases_submesh03.c \
	test_cases_submesh04.c \
	test_cases_key_rotation.c

run_blackbox_tests_LDADD = ../../../src/libmeshlink.la $(LXC_LIBS) $(CMOCKA_LIBS)
run_blackbox_tests_CFLAGS = -D_GNU_SOURCE $(LXC_CFLAGS) $(CMOCKA_CFLAGS)

