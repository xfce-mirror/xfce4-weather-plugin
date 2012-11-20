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
    GtkWidget *combo_unit_system;
    GtkWidget *txt_lat;
    GtkWidget *txt_lon;
    GtkWidget *txt_loc_name;
    GtkWidget *spin_forecast_days;

    GtkWidget *tooltip_yes;
    GtkWidget *tooltip_no;

    GtkWidget *opt_xmloption;
    GtkWidget *lst_xmloption;
    GtkListStore *mdl_xmloption;

    GtkWidget *chk_animate_transition;

    xfceweather_data *wd;
} xfceweather_dialog;


xfceweather_dialog *create_config_dialog(xfceweather_data *data,
                                         GtkWidget *vbox);

void set_callback_config_dialog(xfceweather_dialog *dialog,
                                void (cb) (xfceweather_data *data));

void apply_options(xfceweather_dialog *dialog);

G_END_DECLS

#endif
