plugindir = @XFCE4_PANEL_PLUGINSDIR@

plugin_LTLIBRARIES =							\
	libweather.la

libweather_la_LDFLAGS =							\
	-avoid-version							\
	-export-dynamic							\
	-module 							
	

libweather_la_SOURCES =							\
				config_dialog.c							\
				config_dialog.h							\
				get_data.c									\
				get_data.h 									\
				http_client.c								\
				http_client.h								\
				parsers.c										\
				parsers.h										\
				plugin.c										\
				plugin.h										\
				scrollbox.c									\
				scrollbox.h									\
				summary_window.c						\
				summary_window.h						\
				icon.c											\
				icon.h											\
				search_dialog.c							\
				search_dialog.h							\
				debug_print.h

libweather_la_CFLAGS =							\
	@XFCE4_PANEL_CFLAGS@ 							\
	-DTHEMESDIR=\"$(datadir)/xfce4/weather/icons\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\"

libweather_la_LIBADD = 							\
	@XFCE4_PANEL_LIBS@								\
	$(XML_LIBS_LIBS)
