## Produce this file with automake to get Makefile.in

sbin_PROGRAMS = sptps_test

if LINUX
sbin_PROGRAMS += sptps_speed
endif

DEFAULT_INCLUDES =

sptps_test_SOURCES = \
	logger.c logger.h \
	sptps.c sptps.h \
	sptps_test.c \
	utils.c utils.h

sptps_speed_SOURCES = \
	logger.c logger.h \
	sptps.c sptps.h \
	sptps_speed.c \
	utils.c utils.h

lib_LTLIBRARIES = libmeshlink.la

libmeshlink_la_SOURCES = \
	libmeshlink.c libmeshlink.h \
	buffer.c buffer.h \
	cipher.h \
	conf.c conf.h \
	connection.c connection.h \
	control.c control.h \
	control_common.h \
	crypto.h \
	digest.h \
	dropin.c dropin.h \
	ecdh.h \
	ecdsa.h \
	ecdsagen.h \
	edge.c edge.h \
	ethernet.h \
	event.c event.h \
	fake-gai-errnos.h \
	fake-getaddrinfo.c fake-getaddrinfo.h \
	fake-getnameinfo.c fake-getnameinfo.h \
	getopt.c getopt.h \
	getopt1.c \
	graph.c graph.h \
	hash.c hash.h \
	have.h \
	ipv4.h \
	ipv6.h \
	list.c list.h \
	logger.c logger.h \
	meta.c meta.h \
	names.c names.h \
	net.c net.h \
	net_packet.c \
	net_setup.c \
	net_socket.c \
	netutl.c netutl.h \
	node.c node.h \
	prf.h \
	process.c process.h \
	protocol.c protocol.h \
	protocol_auth.c \
	protocol_edge.c \
	protocol_key.c \
	protocol_misc.c \
	protocol_subnet.c \
	route.c route.h \
	rsa.h \
	rsagen.h \
	script.c script.h \
	splay_tree.c splay_tree.h \
	sptps.c sptps.h \
	subnet.c subnet.h \
	subnet_parse.c \
	system.h \
	tincd.c \
	utils.c utils.h \
	xalloc.h

libmeshlink_la_CFLAGS = -fPIC

libmeshlink_la_LIBADD = -lpthread

libmeshlink_la_SOURCES += \
	openssl/cipher.c \
	openssl/crypto.c \
	openssl/digest.c openssl/digest.h \
	openssl/ecdh.c \
	openssl/ecdsa.c \
	openssl/ecdsagen.c \
	openssl/prf.c \
	openssl/rsa.c \
	openssl/rsagen.c
sptps_test_SOURCES += \
	openssl/cipher.c \
	openssl/crypto.c \
	openssl/digest.c openssl/digest.h \
	openssl/ecdh.c \
	openssl/ecdsa.c \
	openssl/prf.c
sptps_speed_SOURCES += \
	openssl/cipher.c \
	openssl/crypto.c \
	openssl/digest.c openssl/digest.h \
	openssl/ecdh.c \
	openssl/ecdsa.c \
	openssl/ecdsagen.c \
	openssl/prf.c

sptps_speed_LDADD = -lrt

LIBS = @LIBS@

AM_CFLAGS = -DCONFDIR=\"$(sysconfdir)\" -DLOCALSTATEDIR=\"$(localstatedir)\" -DSBINDIR=\"$(sbindir)\"
