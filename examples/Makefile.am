noinst_PROGRAMS = meshlinkapp chat chatpp manynodes channels monitor

AM_CPPFLAGS = $(PTHREAD_CFLAGS) -I${top_srcdir}/src -iquote. -Wall
AM_LDFLAGS = $(PTHREAD_LIBS)

meshlinkapp_SOURCES = meshlinkapp.c
meshlinkapp_LDADD = ${top_builddir}/src/libmeshlink.la

chat_SOURCES = chat.c
chat_LDADD = ${top_builddir}/src/libmeshlink.la

chatpp_SOURCES = chatpp.cc
chatpp_LDADD = ${top_builddir}/src/libmeshlink.la

manynodes_SOURCES = manynodes.c
manynodes_LDADD = ${top_builddir}/src/libmeshlink.la

channels_SOURCES = channels.c
channels_LDADD = ${top_builddir}/src/libmeshlink.la

if CURSES
monitor_SOURCES = monitor.c
monitor_LDADD = ${top_builddir}/src/libmeshlink.la $(NCURSES_LIBS) -lm
endif
