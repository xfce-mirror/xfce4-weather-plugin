@SET_MAKE@

SUBDIRS = panel-plugin icons

EXTRA_DIST =								\
	README

distclean-local:
	rm -rf *.cache *~

rpm: dist
	rpmbuild -ta $(PACKAGE)-$(VERSION).tar.gz
	@rm -f $(PACKAGE)-$(VERSION).tar.gz
