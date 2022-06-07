ACLOCAL_AMFLAGS = -I config

SUBDIRS = . src

# dist_bin_SCRIPTS = dyadrun

install-dyadrun:
	$(SED) -e 's,[@]libdir[@],$(libdir),' dyadrun.in > $(bindir)/dyadrun

uninstall-dyadrun:
	rm $(bindir)/dyadrun
