## Process this file with automake to get Makefile.in

info_TEXINFOS = tinc.texi

man_MANS =

EXTRA_DIST = tincinclude.texi.in

CLEANFILES = *.html tincinclude.texi

# Use `ginstall' in the definition of man_MANS to avoid
# confusion with the `install' target.  The install rule transforms `ginstall'
# to install before applying any user-specified name transformations.
transform = s/ginstall/install/; @program_transform_name@

# For additional rules usually of interest only to the maintainer,
# see GNUmakefile and Makefile.maint.

substitute = sed \
	-e s,'@PACKAGE\@',"$(PACKAGE)",g \
	-e s,'@VERSION\@',"$(VERSION)",g \
	-e s,'@sysconfdir\@',"$(sysconfdir)",g \
	-e s,'@localstatedir\@',"$(localstatedir)",g

tincinclude.texi: tincinclude.texi.in
	$(substitute) $? > $@
	
tinc.texi: tincinclude.texi
