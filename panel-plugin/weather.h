/*  Copyright (c) 2003-2014 Xfce Development Team
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
#ifdef HAVE_UPOWER_GLIB
#include <upower.h>
#endif
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
    time_t last;
    time_t next;
    guint attempt;
    guint check_interval;
    gboolean started;
    gboolean finished;
    guint http_status_code;
} update_info;

typedef struct {
    XfcePanelPlugin *plugin;

#ifdef HAVE_UPOWER_GLIB
    UpClient *upower;
    gboolean upower_on_battery;
    gboolean upower_lid_closed;
#endif
    gboolean power_saving;
    SoupSession *session;
    gchar *geonames_username;

    GtkWidget *button;
    GtkWidget *alignbox;
    GtkWidget *vbox_center_scrollbox;
    GtkWidget *iconimage;
    GdkPixbuf *tooltip_icon;
    GtkWidget *summary_window;
    summary_details *summary_details;
    guint config_remember_tab;
    guint summary_remember_tab;

    gint panel_size;
    guint panel_rows;
    GtkOrientation panel_orientation;
    gboolean single_row;
    xml_weather *weatherdata;
    GArray *astrodata;
    xml_astro *current_astro;

    update_info *astro_update;
    update_info *weather_update;
    update_info *conditions_update;
    time_t next_wakeup;
    gchar *next_wakeup_reason;
    guint update_timer;
    guint summary_update_timer;

    GtkWidget *scrollbox;
    gboolean show_scrollbox;
    gint scrollbox_lines;
    gchar *scrollbox_font;
    GdkColor scrollbox_color;
    gboolean scrollbox_use_color;
    gboolean scrollbox_animate;
    GArray *labels;

    gchar *location_name;
    gchar *lat;
    gchar *lon;
    gint msl;
    gchar *timezone;
    gchar *timezone_initial;
    gint cache_file_max_age;
    gboolean night_time;

    units_config *units;

    icon_theme *icon_theme;
    tooltip_styles tooltip_style;
    forecast_layouts forecast_layout;
    gint forecast_days;
    gboolean round;
} plugin_data;


extern gboolean debug_mode;

void weather_http_queue_request(SoupSession *session,
                                const gchar *uri,
                                SoupSessionCallback callback_func,
                                gpointer user_data);

void scrollbox_set_visible(plugin_data *data);

void forecast_click(GtkWidget *widget,
                    gpointer user_data);

gchar *get_cache_directory(void);

void update_timezone(plugin_data *data);

void update_icon(plugin_data *data);

void update_scrollbox(plugin_data *data,
                      gboolean immediately);

void update_weatherdata_with_reset(plugin_data *data);

GArray *labels_clear(GArray *array);

#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
gboolean xfceweather_set_mode(XfcePanelPlugin *panel,
                              XfcePanelPluginMode mode,
                              plugin_data *data);
#endif

G_END_DECLS

#endif
