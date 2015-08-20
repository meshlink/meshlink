## Produce this file with automake to get Makefile.in

AM_CFLAGS=-I$(top_srcdir)/include

sbin_PROGRAMS = sptps_test sptps_keypair

if LINUX
sbin_PROGRAMS += sptps_speed
endif

DEFAULT_INCLUDES =

ed25519_SOURCES = \
	ed25519/add_scalar.c \
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
	utcp/utcp.c utcp/utcp.h

sptps_test_SOURCES = \
	crypto.c crypto.h \
	logger.c logger.h \
	prf.c prf.h \
	sptps.c sptps.h \
	sptps_test.c \
	utils.c utils.h \
	$(ed25519_SOURCES) \
	$(chacha_poly1305_SOURCES)

sptps_keypair_SOURCES = \
	crypto.c crypto.h \
	sptps_keypair.c \
	utils.c utils.h \
	$(ed25519_SOURCES)

sptps_speed_SOURCES = \
	crypto.c crypto.h \
	logger.c logger.h \
	prf.c prf.h \
	sptps.c sptps.h \
	sptps_speed.c \
	utils.c utils.h \
	$(ed25519_SOURCES) \
	$(chacha_poly1305_SOURCES)

lib_LTLIBRARIES = libmeshlink.la

# -lz passed by LDFLAGS to allow link static lib to dynamic build with libtool (windows)
libmeshlink_la_LDFLAGS = -export-symbols-regex '^(meshlink_|devtool_)' -Wl,-lz

libmeshlink_la_SOURCES = \
	meshlink.c \
	buffer.c buffer.h \
	cipher.h \
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
	fake-gai-errnos.h \
	fake-getaddrinfo.c fake-getaddrinfo.h \
	fake-getnameinfo.c fake-getnameinfo.h \
	graph.c graph.h \
	hash.c hash.h \
	have.h \
	list.c list.h \
	logger.c logger.h \
	meta.c meta.h \
	net.c net.h \
	net_packet.c \
	net_setup.c \
	net_socket.c \
	netutl.c netutl.h \
	node.c \
	prf.c prf.h \
	protocol.c protocol.h \
	protocol_auth.c \
	protocol_edge.c \
	protocol_key.c \
	protocol_misc.c \
	route.c route.h \
	splay_tree.c splay_tree.h \
	sptps.c sptps.h \
	system.h \
	utils.c utils.h \
	xalloc.h \
	devtools.c devtools.h \
	$(ed25519_SOURCES) \
	$(chacha_poly1305_SOURCES) \
	$(utcp_SOURCES)

if MINGW
AM_CFLAGS += -I../catta/include/catta/compat/windows
libmeshlink_la_LDFLAGS += -no-undefined -Wl,--kill-at,--output-def=.libs/libmeshlink-0.def
endif

libmeshlink_la_CFLAGS = $(AM_CFLAGS) -fPIC -I../catta/include/

libmeshlink_la_LIBADD = -lpthread ../catta/src/libcatta.la

libmeshlink_la_SOURCES += \
	ed25519/ecdh.c \
	ed25519/ecdsa.c \
	ed25519/ecdsagen.c
sptps_test_SOURCES += \
	ed25519/ecdh.c \
	ed25519/ecdsa.c
sptps_keypair_SOURCES += \
	ed25519/ecdsagen.c
sptps_speed_SOURCES += \
	ed25519/ecdh.c \
	ed25519/ecdsa.c \
	ed25519/ecdsagen.c

sptps_speed_LDADD = -lrt

LIBS = @LIBS@

AM_CFLAGS += -DCONFDIR=\"$(sysconfdir)\" -DLOCALSTATEDIR=\"$(localstatedir)\" -DSBINDIR=\"$(sbindir)\"
