ACLOCAL_AMFLAGS = -I config

SUBDIRS = . src

# dist_bin_SCRIPTS = dyadrun

install-exec-local:
	$(SED) -e 's,[@]libdir[@],$(libdir),' dyadrun.in > $(bindir)/dyadrun

uninstall-local:
	rm $(bindir)/dyadrun
