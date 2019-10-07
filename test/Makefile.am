TESTS = \
	basic \
	basicpp \
	channels \
	channels-aio \
	channels-aio-fd \
	channels-cornercases \
	channels-failure \
	channels-fork \
	channels-no-partial \
	channels-udp \
	duplicate \
	encrypted \
	ephemeral \
	import-export \
	invite-join \
	sign-verify \
	trio \
	trio2

if BLACKBOX_TESTS
SUBDIRS = blackbox
endif

dist_check_SCRIPTS = $(TESTS)

AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I${top_srcdir}/src -iquote. -Wall
AM_LDFLAGS = $(PTHREAD_LIBS)

check_PROGRAMS = \
	basic \
	basicpp \
	channels \
	channels-aio \
	channels-aio-fd \
	channels-cornercases \
	channels-failure \
	channels-fork \
	channels-no-partial \
	channels-udp \
	duplicate \
	echo-fork \
	encrypted \
	ephemeral \
	import-export \
	invite-join \
	sign-verify \
	trio \
	trio2

if INSTALL_TESTS
bin_PROGRAMS = $(check_PROGRAMS)
endif

basic_SOURCES = basic.c utils.c utils.h
basic_LDADD = ../src/libmeshlink.la

basicpp_SOURCES = basicpp.cpp utils.c utils.h
basicpp_LDADD = ../src/libmeshlink.la

channels_SOURCES = channels.c utils.c utils.h
channels_LDADD = ../src/libmeshlink.la

channels_aio_SOURCES = channels-aio.c utils.c utils.h
channels_aio_LDADD = ../src/libmeshlink.la

channels_aio_fd_SOURCES = channels-aio-fd.c utils.c utils.h
channels_aio_fd_LDADD = ../src/libmeshlink.la

channels_no_partial_SOURCES = channels-no-partial.c utils.c utils.h
channels_no_partial_LDADD = ../src/libmeshlink.la

channels_failure_SOURCES = channels-failure.c utils.c utils.h
channels_failure_LDADD = ../src/libmeshlink.la

channels_fork_SOURCES = channels-fork.c utils.c utils.h
channels_fork_LDADD = ../src/libmeshlink.la

channels_cornercases_SOURCES = channels-cornercases.c utils.c utils.h
channels_cornercases_LDADD = ../src/libmeshlink.la

channels_udp_SOURCES = channels-udp.c utils.c utils.h
channels_udp_LDADD = ../src/libmeshlink.la

duplicate_SOURCES = duplicate.c utils.c utils.h
duplicate_LDADD = ../src/libmeshlink.la

echo_fork_SOURCES = echo-fork.c utils.c utils.h
echo_fork_LDADD = ../src/libmeshlink.la

encrypted_SOURCES = encrypted.c utils.c utils.h
encrypted_LDADD = ../src/libmeshlink.la

ephemeral_SOURCES = ephemeral.c utils.c utils.h
ephemeral_LDADD = ../src/libmeshlink.la

import_export_SOURCES = import-export.c utils.c utils.h
import_export_LDADD = ../src/libmeshlink.la

invite_join_SOURCES = invite-join.c utils.c utils.h
invite_join_LDADD = ../src/libmeshlink.la

sign_verify_SOURCES = sign-verify.c utils.c utils.h
sign_verify_LDADD = ../src/libmeshlink.la

trio_SOURCES = trio.c utils.c utils.h
trio_LDADD = ../src/libmeshlink.la

trio2_SOURCES = trio2.c utils.c utils.h
trio2_LDADD = ../src/libmeshlink.la
