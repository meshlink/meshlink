check_PROGRAMS = gen_invite
SUBDIRS = \
	run_blackbox_tests     \
	test_case_meta_conn_01 \
	test_case_meta_conn_02 \
	test_case_meta_conn_03 \
	test_case_meta_conn_04 \
	test_case_meta_conn_05 \
	test_case_set_status_cb_01 \
	test_case_set_status_cb_02 \
	test_case_set_log_cb_01 \
	test_case_set_log_cb_02 \
	test_case_join_01 \
	test_case_join_02 \
	test_case_join_04 \
	test_case_join_05

gen_invite_SOURCES = util/gen_invite.c common/common_handlers.c common/test_step.c common/mesh_event_handler.c
gen_invite_LDADD = ../../src/libmeshlink.la
gen_invite_CFLAGS = -D_GNU_SOURCE
