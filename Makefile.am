ACLOCAL_AMFLAGS = -I config

SUBDIRS = . src

EXTRA_DIST= \
        NOTICE \
        README.md

dist_bin_SCRIPTS = dyadrun

dyadrun:
	$(SED) -e 's,[@]libdir[@],$(DESTDIR)$(libdir),' dyadrun.in > $@\
		&& chmod u+x $@

clean-local:
	rm -f dyadrun

uninstall-local:
	rm -f $(DESTDIR)$(bindir)/dyadrun
