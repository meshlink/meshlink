## Process this file with automake to get Makefile.in

AUTOMAKE_OPTIONS = gnu

if CATTA
MAYBE_CATTA = catta
endif

SUBDIRS = $(MAYBE_CATTA) src test examples

DIST_SUBDIRS = $(SUBDIRS)

ACLOCAL_AMFLAGS = -I m4 

EXTRA_DIST = \
	COPYING.README \
	README.android \
	$(DX_CONFIG)

@DX_RULES@

MOSTLYCLEANFILES = $(DX_CLEANFILES)

ChangeLog:
	git log > ChangeLog

astyle:
	@astyle --version | grep -q "Version 3" || (echo 'ERROR: astyle version 3 required!' 1>&2 && exit 1)
	astyle --options=.astylerc -nQ \
		src/*.[ch] \
		src/ed25519/e*.[ch] \
		examples/*.[ch] \
		examples/*.cc \
		`find test -name '*.[ch]'`
