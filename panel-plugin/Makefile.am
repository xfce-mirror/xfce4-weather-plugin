AM_CPPFLAGS =								\
	-I$(top_srcdir)							\
	-DTHEMESDIR=\"$(datadir)/xfce4/weather/icons\" 			\
	-DGEONAMES_USERNAME=\"@GEONAMES_USERNAME@\" 			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DG_LOG_DOMAIN=\"weather\"

plugin_LTLIBRARIES = libweather.la

plugindir = $(libdir)/xfce4/panel/plugins

libweather_built_sources =						\
	weather-config-resources.c					\
	$(NULL)

libweather_la_SOURCES =							\
	$(libweather_built_sources)					\
	weather.c							\
	weather.h							\
	weather-config.c						\
	weather-config.h						\
	weather-config.ui						\
	weather-data.c							\
	weather-data.h							\
	weather-debug.c							\
	weather-debug.h							\
	weather-icon.c							\
	weather-icon.h							\
	weather-parsers.c						\
	weather-parsers.h						\
	weather-scrollbox.c						\
	weather-scrollbox.h						\
	weather-search.c						\
	weather-search.h						\
	weather-summary.c						\
	weather-summary.h						\
	weather-translate.c						\
	weather-translate.h

libweather_la_CFLAGS =						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(XFCONF_CFLAGS)								\
	$(GTK_CFLAGS)							\
	$(SOUP_CFLAGS)							\
	$(JSON_CFLAGS)							\
	$(UPOWER_GLIB_CFLAGS)						\
	$(LIBXML_CFLAGS)	\
	-DGTK_DISABLE_SINGLE_INCLUDES	\
	-DGDK_DISABLE_DEPRECATED	\
	-DGTK_DISABLE_DEPRECATED	\
	-DGSEAL_ENABLE

libweather_la_LIBADD =						\
	$(LIBM)                                 \
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)						\
	$(XFCONF_LIBS)							\
	$(GTK_LIBS)							\
	$(LIBXML_LIBS)							\
	$(SOUP_LIBS)							\
	$(JSON_LIBS)

libweather_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS) \
	$(UPOWER_GLIB_LIBS)

weather_config_glade_files = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies --sourcedir $(srcdir) $(srcdir)/weather-config.gresource.xml)

weather-config-resources.c: $(srcdir)/weather-config.gresource.xml $(weather_config_glade_files) Makefile
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source $<

BUILT_SOURCES = \
	$(libweather_built_sources)

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = weather.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

$(desktop_DATA): $(desktop_in_files)
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = \
	$(desktop_in_files) \
	meson.build \
	weather-config.gresource.xml \
	$(NULL)

DISTCLEANFILES = \
	$(desktop_DATA)	\
	$(libweather_built_sources) \
	$(NULL)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
