INCLUDES =								\
	-I$(top_srcdir)							\
	-DTHEMESDIR=\"$(datadir)/xfce4/weather/icons\" 			\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"				\
	-DG_LOG_DOMAIN=\"weather\"

plugin_PROGRAMS =							\
	xfce4-weather-plugin

plugindir = $(libexecdir)/xfce4/panel-plugins

xfce4_weather_plugin_SOURCES =						\
	weather.c							\
	weather.h							\
	weather-config.c						\
	weather-config.h						\
	weather-data.c							\
	weather-data.h							\
	weather-http.c							\
	weather-http.h							\
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

xfce4_weather_plugin_CFLAGS =						\
	$(LIBXFCE4PANEL_CFLAGS)						\
	$(LIBXFCE4UI_CFLAGS)						\
	$(LIBXFCE4UTIL_CFLAGS)						\
	$(GTK_CFLAGS)							\
	$(LIBXML_CFLAGS)

xfce4_weather_plugin_LDADD =						\
	$(LIBXFCE4PANEL_LIBS)						\
	$(LIBXFCE4UTIL_LIBS)						\
	$(LIBXFCE4UI_LIBS)						\
	$(GTK_LIBS)							\
	$(LIBXML_LIBS)							\
	$(LIBRESOLV)

desktopdir = $(datadir)/xfce4/panel-plugins
desktop_in_in_files = weather.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
%.desktop.in: %.desktop.in.in
	sed -e "s,\@libexecdir\@,$(libexecdir),g" < $< > $@
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	$(desktop_in_in_files)

CLEANFILES =								\
	$(desktop_in_files)						\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
