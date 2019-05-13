noinst_PROGRAMS = meshlinkapp chat manynodes channels groupchat

AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I${top_srcdir}/src -iquote. -Wall
AM_LDFLAGS = $(PTHREAD_LIBS)

meshlinkapp_SOURCES = meshlinkapp.c
meshlinkapp_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la

chat_SOURCES = chat.c
chat_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la

#noinst_PROGRAMS += chatpp
#chatpp_SOURCES = chatpp.cc
#chatpp_LDADD = ${top_builddir}/src/libmeshlink.la

manynodes_SOURCES = manynodes.c
manynodes_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la

channels_SOURCES = channels.c
channels_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la

if MONITOR
noinst_PROGRAMS += monitor
monitor_SOURCES = monitor.c
monitor_LDADD = ${top_builddir}/src/libmeshlink.la $(NCURSES_LIBS) -lm
endif

groupchat_SOURCES = groupchat.c
groupchat_LDADD = ${top_builddir}/src/libmeshlink.la ${top_builddir}/catta/src/libcatta.la
