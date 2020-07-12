noinst_PROGRAMS = meshlinkapp chat chatpp manynodes channels groupchat

AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I${top_srcdir}/src -iquote. -Wall
AM_LDFLAGS = $(PTHREAD_LIBS)

meshlinkapp_SOURCES = meshlinkapp.c
meshlinkapp_LDADD = ${top_builddir}/src/libmeshlink.la

chat_SOURCES = chat.c
chat_LDADD = ${top_builddir}/src/libmeshlink.la

noinst_PROGRAMS += chatpp
chatpp_SOURCES = chatpp.cc
chatpp_LDADD = ${top_builddir}/src/libmeshlink.la

manynodes_SOURCES = manynodes.c
manynodes_LDADD = ${top_builddir}/src/libmeshlink.la

channels_SOURCES = channels.c
channels_LDADD = ${top_builddir}/src/libmeshlink.la

if MONITOR
noinst_PROGRAMS += monitor
monitor_SOURCES = monitor.c
monitor_LDADD = ${top_builddir}/src/libmeshlink.la $(NCURSES_LIBS) -lm
endif

groupchat_SOURCES = groupchat.c
groupchat_LDADD = ${top_builddir}/src/libmeshlink.la
