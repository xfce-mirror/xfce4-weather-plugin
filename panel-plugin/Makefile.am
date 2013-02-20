AM_CPPFLAGS =								\
	-I$(top_srcdir)							\
	-DTHEMESDIR=\"$(datadir)/xfce4/weather/icons\" 			\
	-DGEONAMES_USERNAME=\"@GEONAMES_USERNAME@\" 			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DG_LOG_DOMAIN=\"weather\"

plugin_LTLIBRARIES = libweather.la

plugindir = $(libdir)/xfce4/panel/plugins

libweather_la_SOURCES =						\
	weather.c							\
	weather.h							\
	weather-config.c						\
	weather-config.h						\
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
	$(GTK_CFLAGS)							\
	$(SOUP_CFLAGS)							\
	$(UPOWER_GLIB_CFLAGS)						\
	$(LIBXML_CFLAGS)

libweather_la_LIBADD =						\
	$(LIBM)                                 \
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)						\
	$(GTK_LIBS)							\
	$(LIBXML_LIBS)							\
	$(SOUP_LIBS)

libweather_la_LDFLAGS = \
	-avoid-version \
	-module \
	-no-undefined \
	-export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
	$(PLATFORM_LDFLAGS) \
	$(UPOWER_GLIB_LIBS)

desktopdir = $(datadir)/xfce4/panel/plugins
desktop_in_files = weather.desktop.in
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = $(desktop_in_files)

CLEANFILES = $(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
