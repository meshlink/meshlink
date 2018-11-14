## Process this file with automake to get Makefile.in

AUTOMAKE_OPTIONS = gnu

SUBDIRS =  catta src doc test examples

ACLOCAL_AMFLAGS = -I m4 

EXTRA_DIST = README.android

ChangeLog:
	git log > ChangeLog

astyle:
	astyle --options=.astylerc -nQ src/*.[ch] src/ed25519/e*.[ch] examples/*.[ch] examples/*.cc test/*.[ch] test/blackbox/run_blackbox_tests/*.[ch] test/blackbox/util/*.[ch] test/blackbox/test_case_meta_conn_01/*.[ch] test/blackbox/test_case_meta_conn_02/*.[ch] test/blackbox/test_case_meta_conn_03/*.[ch] test/blackbox/test_case_meta_conn_04/*.[ch] test/blackbox/test_case_meta_conn_05/*.[ch] test/blackbox/test_case_join_01/*.[ch] test/blackbox/test_case_join_02/*.[ch] test/blackbox/test_case_set_status_cb_01/*.[ch] test/blackbox/test_case_set_status_cb_02/*.[ch] test/blackbox/common/*.[ch]
