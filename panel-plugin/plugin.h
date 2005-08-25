#ifndef PLUGIN_H
#define PLUGIN_H

#include <panel/plugins.h>
#include <gdk/gdk.h>

#include "parsers.h"
#include "get_data.h"

#define XFCEWEATHER_ROOT "weather"
#define DEFAULT_W_THEME "liquid"
#define UPDATE_TIME 1600

struct xfceweather_data {
        GtkWidget *scrollbox;
        GtkWidget *iconimage;
        GtkWidget *tooltipbox;

        GtkWidget *summary_window;

        GArray *labels;

        GtkIconSize iconsize;

        gint size;
        gint updatetimeout;
        
        gchar *location_code;
        
        enum units unit;

        struct xml_weather *weatherdata;

        gchar *proxy_host;
        gint proxy_port;

        gboolean proxy_fromenv;
        /* used for storing the configured 
         * but not active proxy setttings */
        gchar *saved_proxy_host;
        gint saved_proxy_port;
}; 
gboolean check_envproxy(gchar **, gint *);

#endif
