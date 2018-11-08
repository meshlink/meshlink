## Process this file with automake to get Makefile.in

AUTOMAKE_OPTIONS = gnu

if CATTA
MAYBE_CATTA = catta
endif

SUBDIRS = $(MAYBE_CATTA) src doc test examples

DIST_SUBDIRS = $(SUBDIRS)

ACLOCAL_AMFLAGS = -I m4 

EXTRA_DIST = \
	COPYING.README \
	README.android

ChangeLog:
	git log > ChangeLog

astyle:
	astyle --options=.astylerc -nQ \
		src/*.[ch] \
		src/ed25519/e*.[ch] \
		examples/*.[ch] \
		examples/*.cc \
		`find test -name '*.[ch]'`
