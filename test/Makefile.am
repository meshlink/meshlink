TESTS = \
	basic.test \
	basicpp.test \
	channels.test \
	channels-fork.test \
	channels-cornercases.test \
	duplicate.test \
	import-export.test \
	invite-join.test \
	sign-verify.test \
	trio.test

dist_check_SCRIPTS = $(TESTS)

AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I${top_srcdir}/src -iquote. -Wall
AM_LDFLAGS = $(PTHREAD_LIBS)

check_PROGRAMS = \
	basic \
	basicpp \
	channels \
	channels-fork \
	channels-cornercases \
	duplicate \
	echo-fork \
	import-export \
	invite-join \
	sign-verify \
	trio

if INSTALL_TESTS
bin_PROGRAMS = $(check_PROGRAMS)
endif

basic_SOURCES = basic.c
basic_LDADD = ../src/libmeshlink.la

basicpp_SOURCES = basicpp.cpp
basicpp_LDADD = ../src/libmeshlink.la

channels_SOURCES = channels.c
channels_LDADD = ../src/libmeshlink.la

channels_fork_SOURCES = channels-fork.c
channels_fork_LDADD = ../src/libmeshlink.la

channels_cornercases_SOURCES = channels-cornercases.c utils.c utils.h
channels_cornercases_LDADD = ../src/libmeshlink.la

duplicate_SOURCES = duplicate.c
duplicate_LDADD = ../src/libmeshlink.la

echo_fork_SOURCES = echo-fork.c
echo_fork_LDADD = ../src/libmeshlink.la

import_export_SOURCES = import-export.c
import_export_LDADD = ../src/libmeshlink.la

invite_join_SOURCES = invite-join.c
invite_join_LDADD = ../src/libmeshlink.la

sign_verify_SOURCES = sign-verify.c
sign_verify_LDADD = ../src/libmeshlink.la

trio_SOURCES = trio.c
trio_LDADD = ../src/libmeshlink.la
