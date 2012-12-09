/*  Copyright (c) 2003-2012 Xfce Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __WEATHER_H__
#define __WEATHER_H__

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4util/libxfce4util.h>
#include <libsoup/soup.h>
#include "weather-icon.h"

#define PLUGIN_WEBSITE "http://goodies.xfce.org/projects/panel-plugins/xfce4-weather-plugin"
#define MAX_FORECAST_DAYS 10
#define DEFAULT_FORECAST_DAYS 5
#define MAX_SCROLLBOX_LINES 10

G_BEGIN_DECLS

typedef enum {
    TOOLTIP_SIMPLE,
    TOOLTIP_VERBOSE
} tooltip_styles;

typedef enum {
    FC_LAYOUT_CALENDAR,
    FC_LAYOUT_LIST
} forecast_layouts;

typedef struct {
    GdkCursor *hand_cursor;
    GdkCursor *text_cursor;
    GtkWidget *icon_ebox;
    GtkWidget *text_view;
    gboolean on_icon;
} summary_details;

typedef struct {
    XfcePanelPlugin *plugin;

    SoupSession *session;

    GtkWidget *top_vbox;
    GtkWidget *top_hbox;
    GtkWidget *vbox_center_scrollbox;
    GtkWidget *iconimage;
    GtkWidget *tooltipbox;
    GtkWidget *summary_window;
    summary_details *summary_details;

    gint panel_size;
    gint size;
    GtkOrientation orientation;
    GtkOrientation panel_orientation;
    xml_weather *weatherdata;
    xml_astro *astrodata;
    time_t last_astro_update;
    time_t last_data_update;
    time_t last_conditions_update;
    gint updatetimeout;

    GtkWidget *scrollbox;
    gboolean show_scrollbox;
    guint scrollbox_lines;
    gchar *scrollbox_font;
    GdkColor scrollbox_color;
    gboolean scrollbox_use_color;
    gboolean scrollbox_animate;
    GArray *labels;

    gchar *location_name;
    gchar *lat;
    gchar *lon;
    gint msl;
    gint timezone;
    gboolean night_time;

    units_config *units;

    icon_theme *icon_theme;
    tooltip_styles tooltip_style;
    forecast_layouts forecast_layout;
    gint forecast_days;
    gboolean round;
} xfceweather_data;


extern gboolean debug_mode;

void weather_http_queue_request(SoupSession *session,
                                const gchar *uri,
                                SoupSessionCallback callback_func,
                                gpointer user_data);

void scrollbox_set_visible(xfceweather_data *data);

void forecast_click(GtkWidget *widget,
                    gpointer user_data);

gchar *get_cache_directory(void);

void read_cache_file(xfceweather_data *data);

void update_icon(xfceweather_data *data);

void update_scrollbox(xfceweather_data *data);

void update_weatherdata_with_reset(xfceweather_data *data,
                                   gboolean clear);

GArray *labels_clear(GArray *array);

G_END_DECLS

#endif
