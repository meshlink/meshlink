check_PROGRAMS = run_blackbox_tests

run_blackbox_tests_SOURCES = 						\
	run_blackbox_tests.c 									\
	execute_tests.c							  				\
	../common/mesh_event_handler.c 				\
	../common/containers.c   							\
	../common/common_handlers.c 					\
	../common/test_step.c 								\
	test_cases_channel_conn.c
	
run_blackbox_tests_LDADD = ../../../src/libmeshlink.la -llxc -lcmocka
run_blackbox_tests_CFLAGS = -D_GNU_SOURCE -fsanitize=leak

