TESTS = \
	basic.test

dist_check_SCRIPTS = $(TESTS)

AM_CPPFLAGS = -I../src

check_PROGRAMS = basic

basic_SOURCES = basic.c
basic_LDADD = ../src/libmeshlink.la
