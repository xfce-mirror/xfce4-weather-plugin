plugindir = @XFCE4_PANEL_PLUGINSDIR@

plugin_LTLIBRARIES =							\
	libweather.la

libweather_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-module 							
	

libweather_la_SOURCES =							\
	weather.c

libweather_la_CFLAGS =							\
	@XFCE4_PANEL_CFLAGS@ 							\
	-DTHEMESDIR=\"$(datadir)/xfce4/weather/icons\"

libweather_la_LIBADD = 							\
	@XFCE4_PANEL_LIBS@								\
	$(XML_LIBS_LIBS)
