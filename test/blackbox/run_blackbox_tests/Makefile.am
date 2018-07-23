check_PROGRAMS = run_blackbox_tests

run_blackbox_tests_SOURCES = run_blackbox_tests.c test_cases.c execute_tests.c ../common/containers.c ../common/common_handlers.c ../common/test_step.c test_cases_open.c test_cases_start.c test_cases_stop_close.c test_cases_pmtu.c test_cases_get_self.c test_cases_send.c test_cases_get_node.c test_cases_add_addr.c test_cases_get_ex_addr.c test_cases_add_ex_addr.c test_cases_get_port.c test_cases_destroy.c test_cases_export.c test_cases_get_all_nodes.c test_cases_get_fingerprint.c test_cases_invite.c test_cases_rec_cb.c test_cases_set_port.c test_cases_sign.c test_cases_verify.c
run_blackbox_tests_LDADD = ../../../src/libmeshlink.la -llxc -lcmocka
run_blackbox_tests_CFLAGS = -D_GNU_SOURCE
