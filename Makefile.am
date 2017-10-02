## Process this file with automake to get Makefile.in

AUTOMAKE_OPTIONS = gnu

SUBDIRS =  m4 catta src doc test examples

ACLOCAL_AMFLAGS = -I m4 

EXTRA_DIST = README.android

ChangeLog:
	git log > ChangeLog

astyle:
	astyle --options=.astylerc -nQ src/*.[ch] src/ed25519/e*.[ch] examples/*.[ch] examples/*.cc test/*.[ch]
