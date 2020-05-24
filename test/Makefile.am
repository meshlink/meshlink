TESTS = \
	basic \
	basicpp \
	blacklist \
	channels \
	channels-aio \
	channels-aio-cornercases \
	channels-aio-fd \
	channels-cornercases \
	channels-failure \
	channels-fork \
	channels-framed \
	channels-no-partial \
	channels-udp \
	channels-udp-framed \
	duplicate \
	encrypted \
	ephemeral \
	get-all-nodes \
	import-export \
	invite-join \
	sign-verify \
	trio \
	trio2 \
	utcp-benchmark \
	utcp-benchmark-stream

if BLACKBOX_TESTS
SUBDIRS = blackbox
endif

dist_check_SCRIPTS = $(TESTS)

AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I${top_srcdir}/src -iquote. -Wall
AM_LDFLAGS = $(PTHREAD_LIBS)

check_PROGRAMS = \
	basic \
	basicpp \
	blacklist \
	channels \
	channels-aio \
	channels-aio-cornercases \
	channels-aio-fd \
	channels-cornercases \
	channels-failure \
	channels-fork \
	channels-framed \
	channels-no-partial \
	channels-udp \
	channels-udp-framed \
	duplicate \
	echo-fork \
	encrypted \
	ephemeral \
	get-all-nodes \
	import-export \
	invite-join \
	sign-verify \
	stream \
	trio \
	trio2

if INSTALL_TESTS
bin_PROGRAMS = $(check_PROGRAMS)
endif

basic_SOURCES = basic.c utils.c utils.h
basic_LDADD = $(top_builddir)/src/libmeshlink.la

basicpp_SOURCES = basicpp.cpp utils.c utils.h
basicpp_LDADD = $(top_builddir)/src/libmeshlink.la

blacklist_SOURCES = blacklist.c utils.c utils.h
blacklist_LDADD = $(top_builddir)/src/libmeshlink.la

channels_SOURCES = channels.c utils.c utils.h
channels_LDADD = $(top_builddir)/src/libmeshlink.la

channels_framed_SOURCES = channels-framed.c utils.c utils.h
channels_framed_LDADD = $(top_builddir)/src/libmeshlink.la

channels_aio_SOURCES = channels-aio.c utils.c utils.h
channels_aio_LDADD = $(top_builddir)/src/libmeshlink.la

channels_aio_cornercases_SOURCES = channels-aio-cornercases.c utils.c utils.h
channels_aio_cornercases_LDADD = $(top_builddir)/src/libmeshlink.la

channels_aio_fd_SOURCES = channels-aio-fd.c utils.c utils.h
channels_aio_fd_LDADD = $(top_builddir)/src/libmeshlink.la

channels_no_partial_SOURCES = channels-no-partial.c utils.c utils.h
channels_no_partial_LDADD = $(top_builddir)/src/libmeshlink.la

channels_failure_SOURCES = channels-failure.c utils.c utils.h
channels_failure_LDADD = $(top_builddir)/src/libmeshlink.la

channels_fork_SOURCES = channels-fork.c utils.c utils.h
channels_fork_LDADD = $(top_builddir)/src/libmeshlink.la

channels_cornercases_SOURCES = channels-cornercases.c utils.c utils.h
channels_cornercases_LDADD = $(top_builddir)/src/libmeshlink.la

channels_udp_SOURCES = channels-udp.c utils.c utils.h
channels_udp_LDADD = $(top_builddir)/src/libmeshlink.la

channels_udp_framed_SOURCES = channels-udp-framed.c utils.c utils.h
channels_udp_framed_LDADD = $(top_builddir)/src/libmeshlink.la

duplicate_SOURCES = duplicate.c utils.c utils.h
duplicate_LDADD = $(top_builddir)/src/libmeshlink.la

echo_fork_SOURCES = echo-fork.c utils.c utils.h
echo_fork_LDADD = $(top_builddir)/src/libmeshlink.la

encrypted_SOURCES = encrypted.c utils.c utils.h
encrypted_LDADD = $(top_builddir)/src/libmeshlink.la

ephemeral_SOURCES = ephemeral.c utils.c utils.h
ephemeral_LDADD = $(top_builddir)/src/libmeshlink.la

get_all_nodes_SOURCES = get-all-nodes.c utils.c utils.h
get_all_nodes_LDADD = $(top_builddir)/src/libmeshlink.la

import_export_SOURCES = import-export.c utils.c utils.h
import_export_LDADD = $(top_builddir)/src/libmeshlink.la

invite_join_SOURCES = invite-join.c utils.c utils.h
invite_join_LDADD = $(top_builddir)/src/libmeshlink.la

sign_verify_SOURCES = sign-verify.c utils.c utils.h
sign_verify_LDADD = $(top_builddir)/src/libmeshlink.la

trio_SOURCES = trio.c utils.c utils.h
trio_LDADD = $(top_builddir)/src/libmeshlink.la

trio2_SOURCES = trio2.c utils.c utils.h
trio2_LDADD = $(top_builddir)/src/libmeshlink.la
