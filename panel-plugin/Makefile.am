plugindir = $(libexecdir)/xfce4/panel-plugins
plugin_PROGRAMS = xfce4-weather-plugin

xfce4_weather_plugin_CFLAGS =					\
	-DTHEMESDIR=\"$(datadir)/xfce4/weather/icons\" 		\
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"			\
	@LIBXFCE4PANEL_CFLAGS@					\
        @LIBXFCE4UTIL_CFLAGS@                                   \
	@LIBXML_CFLAGS@

xfce4_weather_plugin_LDFLAGS =					\
	@LIBXFCE4PANEL_LIBS@					\
        @LIBXFCE4UTIL_LIBS@                                     \
	@LIBXML_LIBS@
	
xfce4_weather_plugin_SOURCES =					\
	config_dialog.c						\
	config_dialog.h						\
	get_data.c						\
	get_data.h 						\
	http_client.c						\
	http_client.h						\
	parsers.c						\
	parsers.h						\
	plugin.c						\
	plugin.h						\
	scrollbox.c						\
	scrollbox.h						\
	summary_window.c					\
	summary_window.h					\
	icon.c							\
	icon.h							\
	search_dialog.c						\
	search_dialog.h						\
	translate.c						\
	translate.h

# .desktop file
#
# Some automake trickery here. Because we cannot use $(libexecdir) in the
# automake stage, we'll use sed to get the full path into the .desktop file.
# We also need to let intltool merge the translated fields, so we add an
# additional level of indirection: a <name>.desktop.in.in file.
# 
desktop_in_in_files = weather.desktop.in.in
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
	
desktopdir = $(datadir)/xfce4/panel-plugins
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST = 								\
	$(desktop_in_in_files)

DISTCLEANFILES =							\
	$(desktop_DATA) $(desktop_in_files)
	
# get full path into .desktop file
%.desktop.in: %.desktop.in.in
	sed -e "s^@PLUGIN_PATH@^$(libexecdir)/xfce4/panel-plugins^" \
		$< > $@

