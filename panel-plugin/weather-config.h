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

#ifndef __WEATHER_CONFIG_H__
#define __WEATHER_CONFIG_H__

G_BEGIN_DECLS

typedef struct {
    gchar *name;
    data_types number;
} labeloption;

typedef struct {
    GtkWidget *dialog;

    /* location page */
    GtkWidget *text_loc_name;
    GtkWidget *spin_lat;
    GtkWidget *spin_lon;
    GtkWidget *spin_alt;
    GtkWidget *label_alt_unit;
    GtkWidget *spin_timezone;

    /* units page */
    GtkWidget *combo_unit_temperature;
    GtkWidget *combo_unit_pressure;
    GtkWidget *combo_unit_windspeed;
    GtkWidget *combo_unit_precipitations;
    GtkWidget *combo_unit_altitude;

    /* appearance page */
    GtkWidget *combo_icon_theme;
    GtkWidget *combo_tooltip_style;
    GtkWidget *combo_forecast_layout;
    GtkWidget *spin_forecast_days;
    GtkWidget *check_round_values;
    GtkWidget *check_interpolate_data;

    GtkWidget *chk_animate_transition;
    /* scrollbox page */
    GtkWidget *check_scrollbox_show;
    GtkWidget *spin_scrollbox_lines;
    GtkWidget *button_scrollbox_font;
    GtkWidget *button_scrollbox_color;
    GtkWidget *options_datatypes;        /* labels to choose from */
    GtkWidget *list_datatypes;           /* labels to show */
    GtkListStore *model_datatypes;       /* model for labels */
    GtkWidget *check_scrollbox_animate;

    xfceweather_data *wd;
} xfceweather_dialog;


xfceweather_dialog *create_config_dialog(xfceweather_data *data,
                                         GtkWidget *vbox);

void set_callback_config_dialog(xfceweather_dialog *dialog,
                                void (cb) (xfceweather_data *data));

void apply_options(xfceweather_dialog *dialog);

G_END_DECLS

#endif
