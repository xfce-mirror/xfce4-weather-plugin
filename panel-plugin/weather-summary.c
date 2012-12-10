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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4ui/libxfce4ui.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#include "weather-summary.h"
#include "weather-translate.h"
#include "weather-icon.h"


static gboolean
lnk_clicked(GtkTextTag *tag,
            GObject *obj,
            GdkEvent *event,
            GtkTextIter *iter,
            GtkWidget *textview);


#define BORDER 8

#define APPEND_BTEXT(text)                                          \
    gtk_text_buffer_insert_with_tags(GTK_TEXT_BUFFER(buffer),       \
                                     &iter, text, -1, btag, NULL);

#define APPEND_TEXT_ITEM_REAL(text)                 \
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer), \
                           &iter, text, -1);        \
    g_free(value);

#define APPEND_TEXT_ITEM(text, item)                            \
    rawvalue = get_data(conditions, data->units, item, FALSE);  \
    unit = get_unit(data->units, item);                         \
    value = g_strdup_printf("\t%s%s%s%s%s\n",                   \
                            text, text ? ": " : "",             \
                            rawvalue,                           \
                            strcmp(unit, "°") ? " " : "",       \
                            unit);                              \
    g_free(rawvalue);                                           \
    APPEND_TEXT_ITEM_REAL(value);

#define APPEND_LINK_ITEM(prefix, text, url, lnk_tag)                    \
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer),                     \
                           &iter, prefix, -1);                          \
    gtk_text_buffer_insert_with_tags(GTK_TEXT_BUFFER(buffer),           \
                                     &iter, text, -1, lnk_tag, NULL);   \
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer),                     \
                           &iter, "\n", -1);                            \
    g_object_set_data_full(G_OBJECT(lnk_tag), "url", g_strdup(url), g_free); \
    g_signal_connect(G_OBJECT(lnk_tag), "event",                        \
                     G_CALLBACK(lnk_clicked), NULL);

#define ATTACH_DAYTIME_HEADER(title, pos)               \
    if (data->forecast_layout == FC_LAYOUT_CALENDAR)    \
        gtk_table_attach_defaults                       \
            (GTK_TABLE(table),                          \
             add_forecast_header(title, 90.0, &darkbg), \
             0, 1, pos, pos+1);                         \
    else                                                \
        gtk_table_attach_defaults                       \
            (GTK_TABLE(table),                          \
             add_forecast_header(title, 0.0, &darkbg),  \
             pos, pos+1, 0, 1);                         \


static gboolean
lnk_clicked(GtkTextTag *tag,
            GObject *obj,
            GdkEvent *event,
            GtkTextIter *iter,
            GtkWidget *textview)
{
    const gchar *url;
    gchar *str;

    if (event->type == GDK_BUTTON_RELEASE) {
        url = g_object_get_data(G_OBJECT(tag), "url");
        str = g_strdup_printf("exo-open --launch WebBrowser %s", url);
        g_spawn_command_line_async(str, NULL);
        g_free(str);
    } else if (event->type == GDK_LEAVE_NOTIFY)
        gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(obj),
                                                       GTK_TEXT_WINDOW_TEXT),
                              NULL);
    return FALSE;
}


static gboolean
icon_clicked (GtkWidget *widget,
              GdkEventButton *event,
              gpointer user_data)
{
    return lnk_clicked(user_data, NULL, (GdkEvent *) (event), NULL, NULL);
}


static gboolean
view_motion_notify(GtkWidget *widget,
                   GdkEventMotion *event,
                   summary_details *sum)
{
    GtkTextIter iter;
    GtkTextTag *tag;
    GSList *tags;
    GSList *cur;
    gint bx, by;

    if (event->x != -1 && event->y != -1) {
        gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(sum->text_view),
                                              GTK_TEXT_WINDOW_WIDGET,
                                              event->x, event->y, &bx, &by);
        gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(sum->text_view),
                                           &iter, bx, by);
        tags = gtk_text_iter_get_tags(&iter);
        for (cur = tags; cur != NULL; cur = cur->next) {
            tag = cur->data;
            if (g_object_get_data(G_OBJECT(tag), "url")) {
                gdk_window_set_cursor
                    (gtk_text_view_get_window(GTK_TEXT_VIEW(sum->text_view),
                                              GTK_TEXT_WINDOW_TEXT),
                     sum->hand_cursor);
                return FALSE;
            }
        }
    }
    if (!sum->on_icon)
        gdk_window_set_cursor
            (gtk_text_view_get_window(GTK_TEXT_VIEW(sum->text_view),
                                      GTK_TEXT_WINDOW_TEXT),
             sum->text_cursor);
    return FALSE;
}


static gboolean
icon_motion_notify(GtkWidget *widget,
                   GdkEventMotion *event,
                   summary_details *sum)
{
    sum->on_icon = TRUE;
    gdk_window_set_cursor
        (gtk_text_view_get_window(GTK_TEXT_VIEW(sum->text_view),
                                  GTK_TEXT_WINDOW_TEXT),
         sum->hand_cursor);
    return FALSE;
}


static gboolean
view_leave_notify(GtkWidget *widget,
                  GdkEventMotion *event,
                  summary_details *sum)
{
    sum->on_icon = FALSE;
    gdk_window_set_cursor
        (gtk_text_view_get_window(GTK_TEXT_VIEW(sum->text_view),
                                  GTK_TEXT_WINDOW_TEXT),
         sum->text_cursor);
    return FALSE;
}


static void
view_scrolled_cb(GtkAdjustment *adj,
                 summary_details *sum)
{
    gint x, y, x1, y1;

    if (sum->icon_ebox) {
        x1 = sum->text_view->allocation.width - 191 - 15;
        y1 = sum->text_view->requisition.height - 60 - 15;
        gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(sum->text_view),
                                              GTK_TEXT_WINDOW_TEXT,
                                              x1, y1, &x, &y);
        gtk_text_view_move_child(GTK_TEXT_VIEW(sum->text_view),
                                 sum->icon_ebox, x, y);
    }
}


static void
view_size_allocate_cb(GtkWidget *widget,
                      GtkAllocation *allocation,
                      gpointer data)
{
    view_scrolled_cb(NULL, data);
}


static gchar *
get_logo_path(void)
{
    gchar *cache_dir, *logo_path;

    cache_dir = get_cache_directory();
    logo_path = g_strconcat(cache_dir, G_DIR_SEPARATOR_S,
                            "weather_logo.gif", NULL);
    g_free(cache_dir);
    return logo_path;
}


static void
logo_fetched(SoupSession *session,
             SoupMessage *msg,
             gpointer user_data)
{
    if (msg && msg->response_body && msg->response_body->length > 0) {
        gchar *path = get_logo_path();
        GError *error = NULL;
        GdkPixbuf *pixbuf = NULL;
        if (!g_file_set_contents(path, msg->response_body->data,
                                 msg->response_body->length, &error)) {
            g_warning("Error downloading met.no logo image to %s, "
                      "err %s\n", path, error ? error->message : "?");
            g_error_free(error);
            g_free(path);
            return;
        }
        pixbuf = gdk_pixbuf_new_from_file(path, NULL);
        g_free(path);
        if (pixbuf) {
            gtk_image_set_from_pixbuf(GTK_IMAGE(user_data), pixbuf);
            g_object_unref(pixbuf);
        }
    }
}


static GtkWidget *
weather_summary_get_logo(plugin_data *data)
{
    GtkWidget *image = gtk_image_new();
    GdkPixbuf *pixbuf;
    gchar *path = get_logo_path();

    pixbuf = gdk_pixbuf_new_from_file(path, NULL);
    g_free(path);
    if (pixbuf == NULL)
        weather_http_queue_request(data->session,
                                   "http://met.no/filestore/met.no-logo.gif",
                                   logo_fetched, image);
    else {
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
        g_object_unref(pixbuf);
    }
    return image;
}


static GtkWidget *
create_summary_tab(plugin_data *data)
{
    GtkTextBuffer *buffer;
    GtkTextIter iter;
    GtkTextTag *btag, *ltag0, *ltag1;
    GtkWidget *view, *frame, *scrolled, *icon;
    GtkAdjustment *adj;
    GdkColor lnk_color;
    xml_time *conditions;
    const gchar *unit;
    struct tm *start_tm, *end_tm, *point_tm;
    struct tm *sunrise_tm, *sunset_tm, *moonrise_tm, *moonset_tm;
    gchar *value, *rawvalue, *wind;
    gchar interval_start[80], interval_end[80], point[80];
    gchar sunrise[80], sunset[80], moonrise[80], moonset[80];
    summary_details *sum;

    sum = g_slice_new0(summary_details);
    sum->on_icon = FALSE;
    sum->hand_cursor = gdk_cursor_new(GDK_HAND2);
    sum->text_cursor = gdk_cursor_new(GDK_XTERM);
    data->summary_details = sum;

    sum->text_view = view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view), FALSE);
    frame = gtk_frame_new(NULL);
    scrolled = gtk_scrolled_window_new(NULL, NULL);

    gtk_container_add(GTK_CONTAINER(scrolled), view);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_container_set_border_width(GTK_CONTAINER(frame), BORDER);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(frame), scrolled);

    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
    gtk_text_buffer_get_iter_at_offset(GTK_TEXT_BUFFER(buffer), &iter, 0);
    btag = gtk_text_buffer_create_tag(buffer, NULL, "weight",
                                      PANGO_WEIGHT_BOLD, NULL);

    gdk_color_parse("#0000ff", &lnk_color);
    ltag0 = gtk_text_buffer_create_tag(buffer, "lnk0",
                                       "foreground-gdk", &lnk_color, NULL);
    ltag1 = gtk_text_buffer_create_tag(buffer, "lnk1",
                                       "foreground-gdk", &lnk_color, NULL);

    /* head */
    value = g_strdup_printf(_("Weather report for: %s.\n\n"),
                            data->location_name);
    APPEND_BTEXT(value);
    g_free(value);

    conditions = get_current_conditions(data->weatherdata);
    APPEND_BTEXT(_("Coordinates\n"));
    APPEND_TEXT_ITEM(_("Altitude"), ALTITUDE);
    APPEND_TEXT_ITEM(_("Latitude"), LATITUDE);
    APPEND_TEXT_ITEM(_("Longitude"), LONGITUDE);

    APPEND_BTEXT(_("\nTime\n"));
    point_tm = localtime(&conditions->point);
    strftime(point, 80, "%c", point_tm);
    value = g_strdup_printf
        (_("\tTemperature, wind, atmosphere and cloud data apply to:\n\t%s\n"),
         point);
    APPEND_TEXT_ITEM_REAL(value);

    start_tm = localtime(&conditions->start);
    strftime(interval_start, 80, "%c", start_tm);
    end_tm = localtime(&conditions->end);
    strftime(interval_end, 80, "%c", end_tm);
    value = g_strdup_printf
        (_("\n\tPrecipitation and the weather symbol have been calculated\n"
           "\tfor the following time interval:\n"
           "\tStart:\t%s\n"
           "\tEnd:\t%s\n"),
         interval_start,
         interval_end);
    APPEND_TEXT_ITEM_REAL(value);

    /* sun and moon */
    APPEND_BTEXT(_("\nAstronomical Data\n"));
    if (data->astrodata) {
        if (data->astrodata->sun_never_rises) {
            value = g_strdup(_("\tSunrise:\t\tThe sun never rises today.\n"));
            APPEND_TEXT_ITEM_REAL(value);
        } else if (data->astrodata->sun_never_sets) {
            value = g_strdup(_("\tSunset:\t\tThe sun never sets today.\n"));
            APPEND_TEXT_ITEM_REAL(value);
        } else {
            sunrise_tm = localtime(&data->astrodata->sunrise);
            strftime(sunrise, 80, "%c", sunrise_tm);
            value = g_strdup_printf(_("\tSunrise:\t\t%s\n"), sunrise);
            APPEND_TEXT_ITEM_REAL(value);

            sunset_tm = localtime(&data->astrodata->sunset);
            strftime(sunset, 80, "%c", sunset_tm);
            value = g_strdup_printf(_("\tSunset:\t\t%s\n\n"), sunset);
            APPEND_TEXT_ITEM_REAL(value);
        }

        if (data->astrodata->moon_phase)
            value = g_strdup_printf(_("\tMoon phase:\t%s\n"),
                                    translate_moon_phase
                                    (data->astrodata->moon_phase));
        else
            value = g_strdup(_("\tMoon phase:\tUnknown\n"));
        APPEND_TEXT_ITEM_REAL(value);

        if (data->astrodata->moon_never_rises) {
            value = g_strdup(_("\tMoonrise:\tThe moon never rises today.\n"));
            APPEND_TEXT_ITEM_REAL(value);
        } else if (data->astrodata->moon_never_sets) {
            value = g_strdup(_("\tMoonset:\tThe moon never sets today.\n"));
            APPEND_TEXT_ITEM_REAL(value);
        } else {
            moonrise_tm = localtime(&data->astrodata->moonrise);
            strftime(moonrise, 80, "%c", moonrise_tm);
            value = g_strdup_printf(_("\tMoonrise:\t%s\n"), moonrise);
            APPEND_TEXT_ITEM_REAL(value);

            moonset_tm = localtime(&data->astrodata->moonset);
            strftime(moonset, 80, "%c", moonset_tm);
            value = g_strdup_printf(_("\tMoonset:\t%s\n"), moonset);
            APPEND_TEXT_ITEM_REAL(value);
        }
    } else {
        value = g_strdup(_("\tData not available, will use sane "
                           "default values for night and day.\n"));
        APPEND_TEXT_ITEM_REAL(value);
    }

    /* temperature */
    APPEND_BTEXT(_("\nTemperature\n"));
    APPEND_TEXT_ITEM(_("Temperature"), TEMPERATURE);

    /* wind */
    APPEND_BTEXT(_("\nWind\n"));
    wind = get_data(conditions, data->units, WIND_SPEED, FALSE);
    rawvalue = get_data(conditions, data->units, WIND_BEAUFORT, FALSE);
    value = g_strdup_printf(_("\t%s: %s %s (%s on the Beaufort scale)\n"),
                            _("Speed"), wind,
                            get_unit(data->units, WIND_SPEED),
                            rawvalue);
    g_free(rawvalue);
    g_free(wind);
    APPEND_TEXT_ITEM_REAL(value);

    rawvalue = get_data(conditions, data->units, WIND_DIRECTION, FALSE);
    wind = translate_wind_direction(rawvalue);
    g_free(rawvalue);
    rawvalue = get_data(conditions, data->units, WIND_DIRECTION_DEG, FALSE);
    value = g_strdup_printf("\t%s: %s (%s%s)\n", _("Direction"),
                            wind, rawvalue,
                            get_unit(data->units, WIND_DIRECTION_DEG));
    g_free(rawvalue);
    g_free(wind);
    APPEND_TEXT_ITEM_REAL(value);

    /* precipitation */
    APPEND_BTEXT(_("\nPrecipitations\n"));
    APPEND_TEXT_ITEM(_("Precipitations amount"), PRECIPITATIONS);

    /* atmosphere */
    APPEND_BTEXT(_("\nAtmosphere\n"));
    APPEND_TEXT_ITEM(_("Pressure"), PRESSURE);
    APPEND_TEXT_ITEM(_("Humidity"), HUMIDITY);

    /* clouds */
    APPEND_BTEXT(_("\nClouds\n"));
    APPEND_TEXT_ITEM(_("Fog"), FOG);
    APPEND_TEXT_ITEM(_("Low clouds"), CLOUDS_LOW);
    APPEND_TEXT_ITEM(_("Medium clouds"), CLOUDS_MED);
    APPEND_TEXT_ITEM(_("High clouds"), CLOUDS_HIGH);
    APPEND_TEXT_ITEM(_("Cloudiness"), CLOUDINESS);

    APPEND_BTEXT(_("\nData from The Norwegian Meteorological Institute\n"));
    value = g_strdup("http://met.no");
    g_object_set_data_full(G_OBJECT(ltag0), "url", value, g_free);
    APPEND_LINK_ITEM("\t", _("Thanks to met.no"), "http://met.no/", ltag1);

    g_signal_connect(G_OBJECT(view), "motion-notify-event",
                     G_CALLBACK(view_motion_notify), sum);
    g_signal_connect(G_OBJECT(view), "leave-notify-event",
                     G_CALLBACK(view_leave_notify), sum);

    icon = weather_summary_get_logo(data);

    if (icon) {
        sum->icon_ebox = gtk_event_box_new();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(sum->icon_ebox), FALSE);
        gtk_container_add(GTK_CONTAINER(sum->icon_ebox), icon);
        gtk_text_view_add_child_in_window(GTK_TEXT_VIEW(view),
                                          sum->icon_ebox,
                                          GTK_TEXT_WINDOW_TEXT, 0, 0);
        gtk_widget_show_all(sum->icon_ebox);
        adj =
            gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled));
        g_signal_connect(G_OBJECT(adj), "value-changed",
                         G_CALLBACK(view_scrolled_cb), sum);
        g_signal_connect(G_OBJECT(view), "size_allocate",
                         G_CALLBACK(view_size_allocate_cb), sum);
        g_signal_connect(G_OBJECT(sum->icon_ebox), "button-release-event",
                         G_CALLBACK(icon_clicked), ltag0);
        g_signal_connect(G_OBJECT(sum->icon_ebox), "enter-notify-event",
                         G_CALLBACK(icon_motion_notify), sum);
        g_signal_connect(G_OBJECT(sum->icon_ebox), "motion-notify-event",
                         G_CALLBACK(icon_motion_notify), sum);
        g_signal_connect(G_OBJECT(sum->icon_ebox), "leave-notify-event",
                         G_CALLBACK(view_leave_notify), sum);
    }

    return frame;
}


static gchar *
get_dayname(gint day)
{
    struct tm fcday_tm;
    time_t now_t = time(NULL), fcday_t;
    gint weekday;

    fcday_tm = *localtime(&now_t);
    fcday_t = time_calc_day(fcday_tm, day);
    weekday = localtime(&fcday_t)->tm_wday;
    switch (day) {
    case 0:
        return g_strdup_printf(_("Today"));
    case 1:
        return g_strdup_printf(_("Tomorrow"));
    default:
        return translate_day(weekday);
    }
}


static GtkWidget *
wrap_forecast_cell(const GtkWidget *widget,
                   const GdkColor *color)
{
    GtkWidget *ebox;

    ebox = gtk_event_box_new();
    if (color == NULL)
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), FALSE);
    else {
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), TRUE);
        gtk_widget_modify_bg(GTK_WIDGET(ebox), GTK_STATE_NORMAL, color);
    }
    gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(widget));
    return ebox;
}


static GtkWidget *
add_forecast_header(const gchar *text,
                    const gdouble angle,
                    const GdkColor *color)
{
    GtkWidget *label, *align;
    gchar *str;

    if (angle)
        align = gtk_alignment_new(1, 1, 0, 1);
    else
        align = gtk_alignment_new(1, 1, 1, 0);
    gtk_container_set_border_width(GTK_CONTAINER(align), 4);

    label = gtk_label_new(NULL);
    gtk_label_set_angle(GTK_LABEL(label), angle);
    str = g_strdup_printf("<span foreground=\"white\"><b>%s</b></span>", text ? text : "");
    gtk_label_set_markup(GTK_LABEL(label), str);
    g_free(str);
    gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(label));
    return wrap_forecast_cell(align, color);
}


static GtkWidget *
add_forecast_cell(plugin_data *data,
                  gint day,
                  gint daytime)
{
    GtkWidget *box, *label, *image;
    GdkPixbuf *icon;
    const GdkColor black = {0, 0x0000, 0x0000, 0x0000};
    gchar *wind_speed, *wind_direction, *value, *rawvalue;
    xml_time *fcdata;

    box = gtk_vbox_new(FALSE, 0);

    fcdata = make_forecast_data(data->weatherdata, day, daytime);
    if (fcdata == NULL)
        return box;

    if (fcdata->location == NULL) {
        xml_time_free(fcdata);
        return box;
    }

    /* symbol */
    rawvalue = get_data(fcdata, data->units, SYMBOL, FALSE);
    icon = get_icon(data->icon_theme, rawvalue, 48, (daytime == NIGHT));
    g_free(rawvalue);
    image = gtk_image_new_from_pixbuf(icon);
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(image), TRUE, TRUE, 0);
    if (G_LIKELY(icon))
        g_object_unref(G_OBJECT(icon));

    /* symbol description */
    rawvalue = get_data(fcdata, data->units, SYMBOL, FALSE);
    value = g_strdup_printf("%s",
                            translate_desc(rawvalue, (daytime == NIGHT)));
    g_free(rawvalue);
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), value);
    if (!(day % 2))
        gtk_widget_modify_fg(GTK_WIDGET(label), GTK_STATE_NORMAL, &black);
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(label), TRUE, TRUE, 0);
    g_free(value);

    /* temperature */
    rawvalue = get_data(fcdata, data->units,
                        TEMPERATURE, data->round);
    value = g_strdup_printf("%s %s", rawvalue,
                            get_unit(data->units, TEMPERATURE));
    g_free(rawvalue);
    label = gtk_label_new(value);
    if (!(day % 2))
        gtk_widget_modify_fg(GTK_WIDGET(label), GTK_STATE_NORMAL, &black);
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(label), TRUE, TRUE, 0);
    g_free(value);

    /* wind direction and speed */
    rawvalue = get_data(fcdata, data->units, WIND_DIRECTION, FALSE);
    wind_direction = translate_wind_direction(rawvalue);
    wind_speed = get_data(fcdata, data->units, WIND_SPEED, data->round);
    value = g_strdup_printf("%s %s %s", wind_direction, wind_speed,
                            get_unit(data->units, WIND_SPEED));
    g_free(wind_speed);
    g_free(wind_direction);
    g_free(rawvalue);
    label = gtk_label_new(value);
    if (!(day % 2))
        gtk_widget_modify_fg(GTK_WIDGET(label), GTK_STATE_NORMAL, &black);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    g_free(value);

    gtk_widget_set_size_request(GTK_WIDGET(box), 150, -1);

    xml_time_free(fcdata);
    return box;
}


static GtkWidget *
make_forecast(plugin_data *data)
{
    GtkWidget *table, *ebox, *box, *align;
    GtkWidget *forecast_box;
    const GdkColor lightbg = {0, 0xeaea, 0xeaea, 0xeaea};
    const GdkColor darkbg = {0, 0x6666, 0x6666, 0x6666};
    gchar *dayname;
    gint i;
    daytime daytime;

    if (data->forecast_layout == FC_LAYOUT_CALENDAR)
        table = gtk_table_new(5, data->forecast_days + 1, FALSE);
    else
        table = gtk_table_new(data->forecast_days + 1, 5, FALSE);

    gtk_table_set_row_spacings(GTK_TABLE(table), 0);
    gtk_table_set_col_spacings(GTK_TABLE(table), 0);

    /* empty upper left corner */
    box = gtk_vbox_new(FALSE, 0);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              wrap_forecast_cell(box, &darkbg),
                              0, 1, 0, 1);

    /* daytime headers */
    ATTACH_DAYTIME_HEADER(_("Morning"), 1);
    ATTACH_DAYTIME_HEADER(_("Afternoon"), 2);
    ATTACH_DAYTIME_HEADER(_("Evening"), 3);
    ATTACH_DAYTIME_HEADER(_("Night"), 4);

    for (i = 0; i < data->forecast_days; i++) {
        /* forecast day headers */
        dayname = get_dayname(i);
        if (data->forecast_layout == FC_LAYOUT_CALENDAR)
            ebox = add_forecast_header(dayname, 0.0, &darkbg);
        else
            ebox = add_forecast_header(dayname, 90.0, &darkbg);
        g_free(dayname);

        if (data->forecast_layout == FC_LAYOUT_CALENDAR)
            gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(ebox),
                                      i+1, i+2, 0, 1);
        else
            gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(ebox),
                                      0, 1, i+1, i+2);

        /* get forecast data for each daytime */
        for (daytime = MORNING; daytime <= NIGHT; daytime++) {
            forecast_box = add_forecast_cell(data, i, daytime);
            align = gtk_alignment_new(0.5, 0.5, 1, 1);
            gtk_container_set_border_width(GTK_CONTAINER(align), 4);
            gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(forecast_box));
            if (i % 2)
                ebox = wrap_forecast_cell(align, NULL);
            else
                ebox = wrap_forecast_cell(align, &lightbg);

            if (data->forecast_layout == FC_LAYOUT_CALENDAR)
                gtk_table_attach_defaults(GTK_TABLE(table),
                                          GTK_WIDGET(ebox),
                                          i+1, i+2, 1+daytime, 2+daytime);
            else
                gtk_table_attach_defaults(GTK_TABLE(table),
                                          GTK_WIDGET(ebox),
                                          1+daytime, 2+daytime, i+1, i+2);
        }
    }
    return table;
}


static GtkWidget *
create_forecast_tab(plugin_data *data)
{
    GtkWidget *ebox, *align, *hbox, *scrolled, *table;
    GdkWindow *window;
    GdkScreen *screen;
    GdkRectangle rect;
    gint monitor_num = 0, h_need, h_max, height;
    gint w_need, w_max, width;

    /* To avoid causing a GDK assertion, determine the monitor
     * geometry using the weather icon window, which has already been
     * realized in contrast to the summary window. Then calculate the
     * maximum height we may use, subtracting some sane value just to
     * be on the safe side. */
    window = GDK_WINDOW(gtk_widget_get_window(GTK_WIDGET(data->iconimage)));
    screen = GDK_SCREEN(gdk_window_get_screen(window));
    if (G_LIKELY(window && screen))
        monitor_num = gdk_screen_get_monitor_at_window(screen, window);
    gdk_screen_get_monitor_geometry(screen, monitor_num, &rect);

    /* calculate maximum width and height */
    h_max = rect.height - 250;
    w_max = rect.width - 50;

    /* calculate needed space using a good arbitrary value */
    if (data->forecast_layout == FC_LAYOUT_CALENDAR) {
        w_need = ((data->forecast_days < 8) ? data->forecast_days : 7) * 142;
        h_need = 500;
    } else {
        w_need = (rect.width <= 720) ? 650 : 700;
        h_need = data->forecast_days * 110;
    }

    /* generate the forecast table */
    table = GTK_WIDGET(make_forecast(data));

    /* generate the containing widgets */
    align = gtk_alignment_new(0.5, 0, 0.5, 0);
    if ((data->forecast_layout == FC_LAYOUT_CALENDAR &&
         w_need < w_max && data->forecast_days < 8) ||
        (data->forecast_layout == FC_LAYOUT_LIST && h_need < h_max)) {
        /* no scroll window needed, just align the contents */
        gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(table));
        gtk_container_set_border_width(GTK_CONTAINER(align), BORDER);
        return align;
    } else {
        /* contents too big, scroll window needed */
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(hbox));

        scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_container_set_border_width(GTK_CONTAINER(scrolled), BORDER);

        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled),
                                              align);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_AUTOMATIC,
                                       GTK_POLICY_AUTOMATIC);

        /* set scroll window size */
        width = (w_need > w_max) ? w_max : w_need;
        height = (h_need > h_max) ? h_max : h_need;
        gtk_widget_set_size_request(GTK_WIDGET(scrolled), width, height);

        ebox = gtk_event_box_new();
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), TRUE);
        gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(scrolled));
        return ebox;
    }
}


static void
summary_dialog_response(const GtkWidget *dlg,
                        const gint response,
                        GtkWidget *window)
{
    if (response == GTK_RESPONSE_ACCEPT)
        gtk_widget_destroy(window);
}


GtkWidget *
create_summary_window(plugin_data *data)
{
    GtkWidget *window, *notebook, *vbox, *hbox, *label;
    gchar *title, *symbol;
    GdkPixbuf *icon;
    xml_time *conditions;

    window = xfce_titled_dialog_new_with_buttons(_("Weather Update"),
                                                 NULL,
                                                 GTK_DIALOG_NO_SEPARATOR,
                                                 GTK_STOCK_CLOSE,
                                                 GTK_RESPONSE_ACCEPT, NULL);
    if (data->location_name != NULL) {
        title = g_strdup_printf(_("Weather report for: %s"),
                                data->location_name);
        xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(window), title);
        g_free(title);
    }

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), vbox, TRUE, TRUE, 0);

    conditions = get_current_conditions(data->weatherdata);

    symbol = get_data(conditions, data->units, SYMBOL, FALSE);
    icon = get_icon(data->icon_theme, symbol, 48, data->night_time);
    g_free(symbol);

    gtk_window_set_icon(GTK_WINDOW(window), icon);

    if (G_LIKELY(icon))
        g_object_unref(G_OBJECT(icon));

    if (data->location_name == NULL || data->weatherdata == NULL ||
        data->weatherdata->current_conditions == NULL) {
        hbox = gtk_hbox_new(FALSE, 0);
        if (data->location_name == NULL)
            label = gtk_label_new(_("Please set a location in the plugin settings."));
        else
            label = gtk_label_new(_("Currently no data available."));
        gtk_box_pack_start(GTK_BOX(hbox), GTK_WIDGET(label),
                           TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(hbox),
                           TRUE, TRUE, 0);
        gtk_window_set_default_size(GTK_WINDOW(window), 500, 400);
    } else {
        notebook = gtk_notebook_new();
        gtk_container_set_border_width(GTK_CONTAINER(notebook), BORDER);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                                 create_forecast_tab(data),
                                 gtk_label_new_with_mnemonic(_("_Forecast")));
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                                 create_summary_tab(data),
                                 gtk_label_new_with_mnemonic(_("_Details")));

        gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
    }

    g_signal_connect(G_OBJECT(window), "response",
                     G_CALLBACK(summary_dialog_response), window);

    return window;
}


void
summary_details_free(summary_details *sum)
{
    g_assert(sum != NULL);
    if (G_UNLIKELY(sum == NULL))
        return;

    sum->icon_ebox = NULL;
    sum->text_view = NULL;
    if (sum->hand_cursor)
        gdk_cursor_unref(sum->hand_cursor);
    sum->hand_cursor = NULL;
    if (sum->text_cursor)
        gdk_cursor_unref(sum->text_cursor);
    sum->text_cursor = NULL;
}
