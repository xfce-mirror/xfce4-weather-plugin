#ifndef PLUGIN_H
#define PLUGIN_H

#include "scrollbox.h"
#include "parsers.h"
#include "get_data.h"
#include "http_client.h"
#include "summary_window.h"
#include "config_dialog.h"
#include "icon.h"

#include <panel/plugins.h>
#include <gdk/gdk.h>


#include <config.h>

#define XFCEWEATHER_ROOT "weather"
#define DEFAULT_W_THEME "liquid"
#define UPDATE_TIME 1800

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
}; 

#endif
