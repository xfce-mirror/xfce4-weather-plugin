@SET_MAKE@

SUBDIRS = panel-plugin po icons

EXTRA_DIST =								\
	INSTALL								\
	NEWS								\
	README

distclean-local:
	rm -rf *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz
