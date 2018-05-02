noinst_PROGRAMS = meshlinkapp chat chatpp manynodes channels

AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I${top_srcdir}/src -iquote. -Wall
AM_LDFLAGS = $(PTHREAD_LIBS)

meshlinkapp_SOURCES = meshlinkapp.c
meshlinkapp_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la

chat_SOURCES = chat.c
chat_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la

chatpp_SOURCES = chatpp.cc
chatpp_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la

manynodes_SOURCES = manynodes.c
manynodes_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la

channels_SOURCES = channels.c
channels_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la
