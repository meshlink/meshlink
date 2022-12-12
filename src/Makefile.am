## Produce this file with automake to get Makefile.in

AM_CPPFLAGS = -Wall

ed25519_SOURCES = \
	ed25519/add_scalar.c \
	ed25519/ecdh.c \
	ed25519/ecdsa.c \
	ed25519/ecdsagen.c \
	ed25519/ed25519.h \
	ed25519/fe.c ed25519/fe.h \
	ed25519/fixedint.h \
	ed25519/ge.c ed25519/ge.h \
	ed25519/key_exchange.c \
	ed25519/keypair.c \
	ed25519/precomp_data.h \
	ed25519/sc.c ed25519/sc.h \
	ed25519/seed.c \
	ed25519/sha512.c ed25519/sha512.h \
	ed25519/sign.c \
	ed25519/verify.c

chacha_poly1305_SOURCES = \
	chacha-poly1305/chacha.c chacha-poly1305/chacha.h \
	chacha-poly1305/chacha-poly1305.c chacha-poly1305/chacha-poly1305.h \
	chacha-poly1305/poly1305.c chacha-poly1305/poly1305.h

utcp_SOURCES = \
	utcp.c utcp.h \
	utcp_priv.h

lib_LTLIBRARIES = libmeshlink.la
EXTRA_PROGRAMS = utcp-test

pkginclude_HEADERS = ../include/meshlink++.h ../include/meshlink.h

libmeshlink_la_LDFLAGS = -export-symbols $(srcdir)/meshlink.sym

libmeshlink_la_SOURCES = \
	adns.c adns.h \
	buffer.c buffer.h \
	conf.c conf.h \
	connection.c connection.h \
	crypto.c crypto.h \
	discovery.c discovery.h \
	dropin.c dropin.h \
	ecdh.h \
	ecdsa.h \
	ecdsagen.h \
	edge.c edge.h \
	event.c event.h \
	graph.c graph.h \
	hash.c hash.h \
	have.h \
	list.c list.h \
	logger.c logger.h \
	mdns.c mdns.h \
	meshlink.c ../include/meshlink.h meshlink.sym \
	meshlink_internal.h \
	meshlink_queue.h \
	meta.c meta.h \
	net.c net.h \
	net_packet.c \
	net_setup.c \
	net_socket.c \
	netutl.c netutl.h \
	node.c node.h \
	submesh.c submesh.h \
	packmsg.h \
	prf.c prf.h \
	protocol.c protocol.h \
	protocol_auth.c \
	protocol_edge.c \
	protocol_key.c \
	protocol_misc.c \
	route.c route.h \
	sockaddr.h \
	splay_tree.c splay_tree.h \
	sptps.c sptps.h \
	system.h \
	utils.c utils.h \
	xalloc.h \
	xoshiro.c xoshiro.h \
	devtools.c devtools.h \
	$(ed25519_SOURCES) \
	$(chacha_poly1305_SOURCES) \
	$(utcp_SOURCES)

utcp_test_SOURCES = \
	utcp-test.c \
	$(utcp_SOURCES)

EXTRA_libmeshlink_la_DEPENDENCIES = $(srcdir)/meshlink.sym

libmeshlink_la_CFLAGS = $(PTHREAD_CFLAGS) -fPIC -iquote. -iquote$(top_srcdir)/include
libmeshlink_la_LDFLAGS += $(PTHREAD_LIBS)

utcp_test_CFLAGS = $(PTHREAD_CFLAGS) -fPIC -iquote. -iquote$(top_srcdir)/include
utcp_test_LDFLAGS = $(PTHREAD_LIBS)
