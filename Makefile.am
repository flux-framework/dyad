ACLOCAL_AMFLAGS = -I config

SUBDIRS = . src

dist_bin_SCRIPTS = dyadrun

dyadrun:
	$(SED) -e 's,[@]libdir[@],$(DESTDIR)$(libdir),' dyadrun.in > $@\
		&& chmod u+x $@
