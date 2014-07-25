TESTS = \
	basic.test \
	import-export.test \
	invite-join.test \
	sign-verify.test

dist_check_SCRIPTS = $(TESTS)

AM_CPPFLAGS = -I../src

check_PROGRAMS = basic import-export invite-join sign-verify

basic_SOURCES = basic.c
basic_LDADD = ../src/libmeshlink.la

import_export_SOURCES = import-export.c
import_export_LDADD = ../src/libmeshlink.la

invite_join_SOURCES = invite-join.c
invite_join_LDADD = ../src/libmeshlink.la

sign_verify_SOURCES = sign-verify.c
sign_verify_LDADD = ../src/libmeshlink.la
