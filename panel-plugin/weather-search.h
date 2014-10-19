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

#include <glib.h>
#include <gmodule.h>
#include <gtk/gtk.h>

#ifndef __WEATHER_SEARCH_H__
#define __WEATHER_SEARCH_H__

G_BEGIN_DECLS

typedef struct {
    GtkWidget *dialog;
    GtkWidget *search_entry;
    GtkWidget *result_list;
    GtkWidget *find_button;
    GtkListStore *result_mdl;
    GtkTreeViewColumn *column;

    gchar *result_lat;
    gchar *result_lon;
    gchar *result_name;

    gchar *last_search;

    SoupSession *session;
} search_dialog;


search_dialog *create_search_dialog(GtkWindow *parent,
                                    SoupSession *session);

gboolean run_search_dialog(search_dialog *dialog);

void weather_search_by_ip(SoupSession *session,
                          void (*gui_cb) (const gchar *loc_name,
                                          const gchar *lat,
                                          const gchar *lon,
                                          const units_config *units,
                                          gpointer user_data),
                          gpointer user_data);

void free_search_dialog(search_dialog *dialog);

G_END_DECLS

#endif
