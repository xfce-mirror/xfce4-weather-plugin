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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <libxfce4ui/libxfce4ui.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#include "weather-search.h"
#include "weather-debug.h"

#define BORDER 8


typedef struct {
    void (*cb) (const gchar *loc_name,
                const gchar *lat,
                const gchar *lon,
                const units_config *units,
                gpointer user_data);
    gpointer user_data;
} geolocation_data;


static void
append_result(GtkListStore *mdl,
              const gchar *lat,
              const gchar *lon,
              const gchar *city)
{
    GtkTreeIter iter;

    gtk_list_store_append(mdl, &iter);
    gtk_list_store_set(mdl, &iter, 0, city, 1, lat, 2, lon, -1);
}


static gchar *
sanitize_str(const gchar *str)
{
    GString *retstr = g_string_sized_new(strlen(str));
    gchar *realstr, c = '\0';

    while ((c = *str++)) {
        if(g_ascii_isspace(c))
            g_string_append(retstr, "%20");
        else
            g_string_append_c(retstr, c);
    }
    realstr = retstr->str;
    g_string_free(retstr, FALSE);
    return realstr;
}


static void
cb_searchdone(SoupSession *session,
              SoupMessage *msg,
              gpointer user_data)
{
    search_dialog *dialog = (search_dialog *) user_data;
    xmlDoc *doc;
    xmlNode *cur_node;
    xml_place *place;
    gint found = 0;
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    gtk_widget_set_sensitive(dialog->find_button, TRUE);

    doc = get_xml_document(msg);
    if (!doc)
        return;

    cur_node = xmlDocGetRootElement(doc);
    if (cur_node) {
        for (cur_node = cur_node->children; cur_node;
             cur_node = cur_node->next) {
            place = parse_place(cur_node);
            weather_dump(weather_dump_place, place);

            if (place && place->lat && place->lon && place->display_name) {
                append_result(dialog->result_mdl,
                              place->lat, place->lon, place->display_name);
                found++;
            }

            if (place) {
                xml_place_free(place);
                place = NULL;
            }
        }
    }
    xmlFreeDoc(doc);

    if (found > 0)
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->result_mdl),
                                          &iter)) {
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW
                                                    (dialog->result_list));
            gtk_tree_selection_select_iter(selection, &iter);
            gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog->dialog),
                                              GTK_RESPONSE_ACCEPT, TRUE);
        }

    gtk_tree_view_column_set_title(dialog->column, _("Results"));
}


static void
search_cb(GtkWidget *widget,
          gpointer user_data)
{
    search_dialog *dialog = (search_dialog *) user_data;
    GtkTreeSelection *selection;
    const gchar *str;
    gchar *sane_str, *url;

    str = gtk_entry_get_text(GTK_ENTRY(dialog->search_entry));
    if (strlen(str) == 0)
        return;

    if (dialog->last_search && !strcmp(str, dialog->last_search)) {
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->result_list));
        if (gtk_tree_selection_count_selected_rows(selection) == 1) {
            gtk_dialog_response(GTK_DIALOG(dialog->dialog), GTK_RESPONSE_ACCEPT);
            return;
        }
    }
    g_free(dialog->last_search);
    dialog->last_search = g_strdup(str);

    gtk_list_store_clear(GTK_LIST_STORE(dialog->result_mdl));

    if ((sane_str = sanitize_str(str)) == NULL)
        return;

    gtk_widget_set_sensitive(dialog->find_button, FALSE);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog->dialog),
                                      GTK_RESPONSE_ACCEPT, FALSE);

    url = g_strdup_printf("http://nominatim.openstreetmap.org/"
                          "search?q=%s&format=xml", sane_str);
    g_free(sane_str);

    gtk_tree_view_column_set_title(dialog->column, _("Searching..."));
    g_message(_("getting %s"), url);
    weather_http_queue_request(dialog->session, url, cb_searchdone, dialog);
    g_free(url);
}


static void
pass_search_results(GtkTreeView *tree_view,
                    GtkTreePath *path,
                    GtkTreeViewColumn *column,
                    GtkDialog *dialog)
{
    gtk_dialog_response(dialog, GTK_RESPONSE_ACCEPT);
}


search_dialog *
create_search_dialog(GtkWindow *parent,
                     SoupSession *session)
{
    search_dialog *dialog;
    GtkWidget *dialog_vbox, *vbox, *hbox, *scroll, *frame;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    dialog = g_slice_new0(search_dialog);

    if (!dialog)
        return NULL;

    dialog->session = session;

    dialog->dialog =
        xfce_titled_dialog_new_with_buttons(_("Search location"),
                                            parent,
                                            GTK_DIALOG_MODAL |
                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_STOCK_CANCEL,
                                            GTK_RESPONSE_REJECT,
                                            GTK_STOCK_OK,
                                            GTK_RESPONSE_ACCEPT,
                                            NULL);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog->dialog),
                                      GTK_RESPONSE_ACCEPT, FALSE);
    gtk_window_set_icon_name(GTK_WINDOW(dialog->dialog), GTK_STOCK_FIND);

    dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog->dialog));

    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
    gtk_box_pack_start(GTK_BOX(dialog_vbox), vbox,
                       TRUE, TRUE, 0);

    xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(dialog->dialog),
                                    _("Enter a city name or address"));

    hbox = gtk_hbox_new(FALSE, BORDER);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    dialog->search_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), dialog->search_entry, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(dialog->search_entry), "activate",
                     G_CALLBACK(search_cb), dialog);

    dialog->find_button = gtk_button_new_from_stock(GTK_STOCK_FIND);
    gtk_box_pack_start(GTK_BOX(hbox), dialog->find_button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(dialog->find_button), "clicked",
                     G_CALLBACK(search_cb), dialog);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(frame), scroll);

    dialog->result_mdl = gtk_list_store_new(3, G_TYPE_STRING,
                                            G_TYPE_STRING, G_TYPE_STRING);
    dialog->result_list =
        gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->result_mdl));
    dialog->column =
        gtk_tree_view_column_new_with_attributes(_("Results"),
                                                 renderer,
                                                 "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(dialog->result_list),
                                dialog->column);
    g_signal_connect(G_OBJECT(dialog->result_list), "row-activated",
                     G_CALLBACK(pass_search_results), dialog->dialog);
    gtk_container_add(GTK_CONTAINER(scroll), dialog->result_list);

    gtk_widget_set_size_request(dialog->dialog, 600, 500);

    return dialog;
}


gboolean
run_search_dialog(search_dialog *dialog)
{
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    GValue value = { 0, };

    gtk_widget_show_all(dialog->dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog->dialog)) == GTK_RESPONSE_ACCEPT) {
        selection =
            gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->result_list));

        if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
            gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->result_mdl),
                                     &iter, 1, &value);
            dialog->result_lat = g_strdup(g_value_get_string(&value));
            g_value_unset(&value);

            gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->result_mdl),
                                     &iter, 2, &value);
            dialog->result_lon = g_strdup(g_value_get_string(&value));
            g_value_unset(&value);

            gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->result_mdl),
                                     &iter, 0, &value);
            dialog->result_name = g_strdup(g_value_get_string(&value));
            g_value_unset(&value);

            return TRUE;
        }
    }
    return FALSE;
}


void
free_search_dialog(search_dialog *dialog)
{
    g_free(dialog->result_lat);
    g_free(dialog->result_lon);
    g_free(dialog->result_name);
    g_free(dialog->last_search);

    gtk_widget_destroy(dialog->dialog);

    g_slice_free(search_dialog, dialog);
}


static units_config *
get_preferred_units(const gchar *country_code)
{
    units_config *units;

    if (G_UNLIKELY(country_code == NULL))
        return NULL;

    units = g_slice_new0(units_config);
    if (G_UNLIKELY(units == NULL))
        return NULL;

    /* List gathered from http://www.maxmind.com/app/iso3166 */
    if (!strcmp(country_code, "US") ||        /* United States */
        !strcmp(country_code, "GB") ||        /* United Kingdom */
        !strcmp(country_code, "JM") ||        /* Jamaica */
        !strcmp(country_code, "LR") ||        /* Liberia */
        !strcmp(country_code, "MM")) {        /* Myanmar(Burma) */
        units->pressure = PSI;
        units->windspeed = MPH;
        units->precipitation = INCHES;
        units->altitude = FEET;
    } else {
        units->pressure = HECTOPASCAL;
        units->windspeed = KMH;
        units->precipitation = MILLIMETERS;
        units->altitude = METERS;
    }

    if (!strcmp(country_code, "US") ||        /* United States */
        !strcmp(country_code, "JM")) {        /* Jamaica */
        units->temperature = FAHRENHEIT;
    } else {
        units->temperature = CELSIUS;
    }

    if (!strcmp(country_code, "RU"))          /* Russian Federation */
        units->pressure = TORR;

    if (!strcmp(country_code, "US"))          /* United States */
        units->apparent_temperature = WINDCHILL_HEATINDEX;
    else if (!strcmp(country_code, "CA"))     /* Canada */
        units->apparent_temperature = WINDCHILL_HUMIDEX;
    else if (!strcmp(country_code, "AU"))     /* Australia */
        units->apparent_temperature = STEADMAN;

    return units;
}


static void
cb_geolocation(SoupSession *session,
               SoupMessage *msg,
               gpointer user_data)
{
    geolocation_data *data = (geolocation_data *) user_data;
    xml_geolocation *geo;
    gchar *full_loc;
    units_config *units;

    geo = (xml_geolocation *)
        parse_xml_document(msg, (XmlParseFunc) parse_geolocation);
    weather_dump(weather_dump_geolocation, geo);

    if (!geo) {
        data->cb(NULL, NULL, NULL, NULL, data->user_data);
        g_free(data);
        return;
    }

    if (geo->country_name && geo->city && strcmp(geo->city, "(none)"))
        if (geo->country_code && !strcmp(geo->country_code, "US") &&
            geo->region_name)
            full_loc = g_strdup_printf("%s, %s", geo->city, geo->region_name);
        else
            full_loc = g_strdup_printf("%s, %s", geo->city, geo->country_name);
    else if (geo->region_name && strcmp(geo->region_name, "(none)"))
        full_loc = g_strdup(geo->region_name);
    else if (geo->country_name && strcmp(geo->country_name, "(none)"))
        full_loc = g_strdup(geo->country_name);
    else if (geo->latitude && geo->longitude) {
        /*
         * TRANSLATORS: Latitude and longitude are known from the
         * search results, but not the location name. The user shall
         * give the place a name.
         */
        full_loc = g_strdup(_("Unnamed place"));
    } else
        full_loc = NULL;

    units = get_preferred_units(geo->country_code);
    weather_dump(weather_dump_units_config, units);
    data->cb(full_loc, geo->latitude, geo->longitude, units, data->user_data);
    g_slice_free(units_config, units);
    xml_geolocation_free(geo);
    g_free(full_loc);
    g_free(data);
}


void weather_search_by_ip(SoupSession *session,
                          void (*gui_cb) (const gchar *loc_name,
                                          const gchar *lat,
                                          const gchar *lon,
                                          const units_config *units,
                                          gpointer user_data),
                          gpointer user_data)
{
    geolocation_data *data;
    const gchar *url = "http://geoip.xfce.org/";

    if (!gui_cb)
        return;

    data = g_new0(geolocation_data, 1);
    data->cb = gui_cb;
    data->user_data = user_data;

    g_message(_("getting %s"), url);
    weather_http_queue_request(session, url, cb_geolocation, data);
}
