TESTS = \
	basic.test \
	basicpp.test \
	channels.test \
	channels-fork.test \
	channels-cornercases.test \
	import-export.test \
	invite-join.test \
	sign-verify.test

dist_check_SCRIPTS = $(TESTS)

AM_CPPFLAGS = -I../src

check_PROGRAMS = \
	basic \
	basicpp \
	channels \
	channels-fork \
	channels-cornercases \
	import-export \
	invite-join \
	sign-verify \
	echo-fork

basic_SOURCES = basic.c
basic_LDADD = ../src/libmeshlink.la

basicpp_SOURCES = basicpp.cpp
basicpp_LDADD = ../src/libmeshlink.la

channels_SOURCES = channels.cpp
channels_LDADD = ../src/libmeshlink.la

channels_fork_SOURCES = channels-fork.cpp
channels_fork_LDADD = ../src/libmeshlink.la

channels_cornercases_SOURCES = channels-cornercases.cpp utils.cpp utils.h
channels_cornercases_LDADD = ../src/libmeshlink.la

echo_fork_SOURCES = echo-fork.c
echo_fork_LDADD = ../src/libmeshlink.la

import_export_SOURCES = import-export.c
import_export_LDADD = ../src/libmeshlink.la

invite_join_SOURCES = invite-join.c
invite_join_LDADD = ../src/libmeshlink.la

sign_verify_SOURCES = sign-verify.c
sign_verify_LDADD = ../src/libmeshlink.la

channels_SOURCES = channels.c
channels_LDADD = ../src/libmeshlink.la
