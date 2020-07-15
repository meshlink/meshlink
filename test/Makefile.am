TESTS = \
	basic \
	basicpp \
	blacklist \
	channels \
	channels-aio \
	channels-aio-abort \
	channels-aio-cornercases \
	channels-aio-fd \
	channels-buffer-storage \
	channels-cornercases \
	channels-failure \
	channels-fork \
	channels-no-partial \
	channels-udp \
	channels-udp-cornercases \
	duplicate \
	encrypted \
	ephemeral \
	get-all-nodes \
	import-export \
	invite-join \
	metering \
	metering-relayed \
	metering-slowping \
	metering-tcponly \		
	meta-connections \
	pmtu \
	sign-verify \
	storage-policy \
	trio \
	trio2 \
	utcp-benchmark \
	utcp-benchmark-stream

TESTS += \
	api_set_node_status_cb

if BLACKBOX_TESTS
SUBDIRS = blackbox
endif

dist_check_SCRIPTS = $(TESTS)

AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I${top_srcdir}/src -iquote. -Wall
AM_LDFLAGS = $(PTHREAD_LIBS)

check_PROGRAMS = \
	api_set_node_status_cb \
	basic \
	basicpp \
	blacklist \
	channels \
	channels-aio \
	channels-aio-abort \
	channels-aio-cornercases \
	channels-aio-fd \
	channels-buffer-storage \
	channels-cornercases \
	channels-failure \
	channels-fork \
	channels-no-partial \
	channels-udp \
	channels-udp-cornercases \
	duplicate \
	echo-fork \
	encrypted \
	ephemeral \
	get-all-nodes \
	import-export \
	invite-join \
	metering \
	metering-relayed \
	metering-slowping \
	metering-tcponly \
	meta-connections \
	pmtu \
	sign-verify \
	storage-policy \
	stream \
	trio \
	trio2

if INSTALL_TESTS
bin_PROGRAMS = $(check_PROGRAMS)
endif

api_set_node_status_cb_SOURCES = api_set_node_status_cb.c utils.c utils.h
api_set_node_status_cb_LDADD = $(top_builddir)/src/libmeshlink.la

basic_SOURCES = basic.c utils.c utils.h
basic_LDADD = $(top_builddir)/src/libmeshlink.la

basicpp_SOURCES = basicpp.cpp utils.c utils.h
basicpp_LDADD = $(top_builddir)/src/libmeshlink.la

blacklist_SOURCES = blacklist.c utils.c utils.h
blacklist_LDADD = $(top_builddir)/src/libmeshlink.la

channels_SOURCES = channels.c utils.c utils.h
channels_LDADD = $(top_builddir)/src/libmeshlink.la

channels_aio_SOURCES = channels-aio.c utils.c utils.h
channels_aio_LDADD = $(top_builddir)/src/libmeshlink.la

channels_aio_abort_SOURCES = channels-aio-abort.c utils.c utils.h
channels_aio_abort_LDADD = $(top_builddir)/src/libmeshlink.la

channels_aio_cornercases_SOURCES = channels-aio-cornercases.c utils.c utils.h
channels_aio_cornercases_LDADD = $(top_builddir)/src/libmeshlink.la

channels_aio_fd_SOURCES = channels-aio-fd.c utils.c utils.h
channels_aio_fd_LDADD = $(top_builddir)/src/libmeshlink.la

channels_buffer_storage_SOURCES = channels-buffer-storage.c utils.c utils.h
channels_buffer_storage_LDADD = $(top_builddir)/src/libmeshlink.la

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

channels_udp_cornercases_SOURCES = channels-udp-cornercases.c utils.c utils.h
channels_udp_cornercases_LDADD = $(top_builddir)/src/libmeshlink.la

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

metering_SOURCES = metering.c netns_utils.c netns_utils.h utils.c utils.h
metering_LDADD = $(top_builddir)/src/libmeshlink.la

metering_relayed_SOURCES = metering-relayed.c netns_utils.c netns_utils.h utils.c utils.h
metering_relayed_LDADD = $(top_builddir)/src/libmeshlink.la

metering_slowping_SOURCES = metering-slowping.c netns_utils.c netns_utils.h utils.c utils.h
metering_slowping_LDADD = $(top_builddir)/src/libmeshlink.la

metering_tcponly_SOURCES = metering-tcponly.c netns_utils.c netns_utils.h utils.c utils.h
metering_tcponly_LDADD = $(top_builddir)/src/libmeshlink.la

meta_connections_SOURCES = meta-connections.c netns_utils.c netns_utils.h utils.c utils.h
meta_connections_LDADD = $(top_builddir)/src/libmeshlink.la

pmtu_SOURCES = pmtu.c utils.c utils.h
pmtu_LDADD = $(top_builddir)/src/libmeshlink.la

sign_verify_SOURCES = sign-verify.c utils.c utils.h
sign_verify_LDADD = $(top_builddir)/src/libmeshlink.la

storage_policy_SOURCES = storage-policy.c utils.c utils.h
storage_policy_LDADD = $(top_builddir)/src/libmeshlink.la

trio_SOURCES = trio.c utils.c utils.h
trio_LDADD = $(top_builddir)/src/libmeshlink.la

trio2_SOURCES = trio2.c utils.c utils.h
trio2_LDADD = $(top_builddir)/src/libmeshlink.la
