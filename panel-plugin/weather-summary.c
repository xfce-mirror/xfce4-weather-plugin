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

/*
 * TRANSLATORS: This format string belongs to the macro used for
 * printing the "Label: Value Unit" lines on the details tab, e.g.
 * "Temperature: 10 °C" or "Latitude: 95.7°".
 * The %s stand for:
 *   - label
 *   - ": " if label is not empty, else empty
 *   - value
 *   - space if unit is not degree "°" (but this is not °C or °F!)
 *   - unit
 * Usually, you should leave this unchanged, BUT...
 * RTL TRANSLATORS: In case you did not translate the measurement
 * unit, use LRM (left-to-right mark) etc. to align it properly with
 * its numeric value.
 */
#define APPEND_TEXT_ITEM(text, item)                        \
    rawvalue = get_data(conditions, data->units, item,      \
                        FALSE, data->night_time);           \
    unit = get_unit(data->units, item);                     \
    value = g_strdup_printf(_("\t%s%s%s%s%s\n"),            \
                            text, text ? ": " : "",         \
                            rawvalue,                       \
                            strcmp(unit, "°") ? " " : "",   \
                            unit);                          \
    g_free(rawvalue);                                       \
    APPEND_TEXT_ITEM_REAL(value);

#define APPEND_LINK_ITEM(prefix, text, url, lnk_tag)                    \
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer),                     \
                           &iter, prefix, -1);                          \
    gtk_text_buffer_insert_with_tags(GTK_TEXT_BUFFER(buffer),           \
                                     &iter, text, -1, lnk_tag, NULL);   \
    gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer),                     \
                           &iter, "\n", -1);                            \
    g_object_set_data_full(G_OBJECT(lnk_tag), "url",                    \
                           g_strdup(url), g_free);                      \
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

#define APPEND_TOOLTIP_ITEM(description, item)                  \
    value = get_data(fcdata, data->units, item,                 \
                     data->round, data->night_time);            \
    if (strcmp(value, "")) {                                    \
        unit = get_unit(data->units, item);                     \
        g_string_append_printf(text, description, value,        \
                               strcmp(unit, "°") ? " " : "",    \
                               unit);                           \
    } else                                                      \
        g_string_append_printf(text, description, "-", "", ""); \
    g_free(value);


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
        /* TRANSLATORS: DO NOT TRANSLATE THIS STRING. This string is
           not visible to the user but controls the alignment of the
           met.no image on the details tab in the summary window,
           which is needed for RTL languages because the text is on
           the right side.
           If you are a RTL translator (hebrew, arabic), set this
           string to "RTL" to align the image to the left.
           If not, leave this string untouched or untranslated.
           Whatever you do, it should not have the value "RTL".
           If you know of a better way to determine LTR/RTL that makes
           this tweak unnecessary, please tell the developer.
        */
        if (!strcmp(_("LTR"), "RTL"))
            x1 = -30;
        else
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
            g_warning(_("Error downloading met.no logo image to %s, "
                        "reason: %s\n"), path,
                      error ? error->message : _("unknown"));
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
                                   "https://met.no/filestore/met.no-logo.gif",
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
    GtkTextTag *btag, *ltag_img, *ltag_metno, *ltag_wiki, *ltag_geonames;
    GtkWidget *view, *frame, *scrolled, *icon;
    GtkAdjustment *adj;
    GdkColor lnk_color;
    xml_time *conditions;
    const gchar *unit;
    gchar *value, *rawvalue, *wind;
    gchar *last_download, *next_download;
    gchar *interval_start, *interval_end, *point;
    gchar *sunrise, *sunset, *moonrise, *moonset;
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

    conditions = get_current_conditions(data->weatherdata);
    APPEND_BTEXT(_("Coordinates\n"));
    APPEND_TEXT_ITEM(_("Altitude"), ALTITUDE);
    APPEND_TEXT_ITEM(_("Latitude"), LATITUDE);
    APPEND_TEXT_ITEM(_("Longitude"), LONGITUDE);

    /* TRANSLATORS: Please use as many \t as appropriate to align the
       date/time values as in the original. */
    APPEND_BTEXT(_("\nDownloads\n"));
    last_download = format_date(data->weather_update->last, NULL, TRUE);
    next_download = format_date(data->weather_update->next, NULL, TRUE);
    value = g_strdup_printf(_("\tWeather data:\n"
                              "\tLast:\t%s\n"
                              "\tNext:\t%s\n"
                              "\tCurrent failed attempts: %d\n\n"),
                            last_download,
                            next_download,
                            data->weather_update->attempt);
    g_free(last_download);
    g_free(next_download);
    APPEND_TEXT_ITEM_REAL(value);

    /* Check for deprecated API and issue a warning if necessary */
    if (data->weather_update->http_status_code == 203)
        APPEND_BTEXT
            (_("\tMet.no LocationforecastLTS API states that this version\n"
               "\tof the webservice is deprecated, and the plugin needs to be\n"
               "\tadapted to use a newer version, or it will stop working within\n"
               "\ta few months.\n"
               "\tPlease file a bug on https://bugzilla.xfce.org if no one\n"
               "\telse has done so yet.\n\n"));

    last_download = format_date(data->astro_update->last, NULL, TRUE);
    next_download = format_date(data->astro_update->next, NULL, TRUE);
    value = g_strdup_printf(_("\tAstronomical data:\n"
                              "\tLast:\t%s\n"
                              "\tNext:\t%s\n"
                              "\tCurrent failed attempts: %d\n"),
                            last_download,
                            next_download,
                            data->astro_update->attempt);
    g_free(last_download);
    g_free(next_download);
    APPEND_TEXT_ITEM_REAL(value);

    /* Check for deprecated sunrise API and issue a warning if necessary */
    if (data->astro_update->http_status_code == 203)
        APPEND_BTEXT
            (_("\n\tMet.no sunrise API states that this version of the webservice\n"
               "\tis deprecated, and the plugin needs to be adapted to use\n"
               "\ta newer version, or it will stop working within a few months.\n"
               "\tPlease file a bug on https://bugzilla.xfce.org if no one\n"
               "\telse has done so yet.\n"));

    /* calculation times */
    APPEND_BTEXT(_("\nTimes Used for Calculations\n"));
    point = format_date(conditions->point, NULL, TRUE);
    value = g_strdup_printf
        (_("\tTemperatures, wind, atmosphere and cloud data calculated\n"
           "\tfor:\t\t%s\n"),
         point);
    g_free(point);
    APPEND_TEXT_ITEM_REAL(value);

    interval_start = format_date(conditions->start, NULL, TRUE);
    interval_end = format_date(conditions->end, NULL, TRUE);
    value = g_strdup_printf
        (_("\n\tPrecipitation and the weather symbol have been calculated\n"
           "\tusing the following time interval:\n"
           "\tStart:\t%s\n"
           "\tEnd:\t%s\n"),
         interval_start,
         interval_end);
    g_free(interval_end);
    g_free(interval_start);
    APPEND_TEXT_ITEM_REAL(value);

    /* sun and moon */
    APPEND_BTEXT(_("\nAstronomical Data\n"));
    if (data->current_astro) {
        if (data->current_astro->sun_never_rises) {
            value = g_strdup(_("\tSunrise:\t\tThe sun never rises today.\n"));
            APPEND_TEXT_ITEM_REAL(value);
        } else if (data->current_astro->sun_never_sets) {
            value = g_strdup(_("\tSunset:\t\tThe sun never sets today.\n"));
            APPEND_TEXT_ITEM_REAL(value);
        } else {
            sunrise = format_date(data->current_astro->sunrise, NULL, TRUE);
            value = g_strdup_printf(_("\tSunrise:\t\t%s\n"), sunrise);
            g_free(sunrise);
            APPEND_TEXT_ITEM_REAL(value);

            sunset = format_date(data->current_astro->sunset, NULL, TRUE);
            value = g_strdup_printf(_("\tSunset:\t\t%s\n\n"), sunset);
            g_free(sunset);
            APPEND_TEXT_ITEM_REAL(value);
        }

        if (data->current_astro->moon_phase)
            value = g_strdup_printf(_("\tMoon phase:\t%s\n"),
                                    translate_moon_phase
                                    (data->current_astro->moon_phase));
        else
            value = g_strdup(_("\tMoon phase:\tUnknown\n"));
        APPEND_TEXT_ITEM_REAL(value);

        if (data->current_astro->moon_never_rises) {
            value =
                g_strdup(_("\tMoonrise:\tThe moon never rises today.\n"));
            APPEND_TEXT_ITEM_REAL(value);
        } else if (data->current_astro->moon_never_sets) {
            value =
                g_strdup(_("\tMoonset:\tThe moon never sets today.\n"));
            APPEND_TEXT_ITEM_REAL(value);
        } else {
            moonrise = format_date(data->current_astro->moonrise, NULL, TRUE);
            value = g_strdup_printf(_("\tMoonrise:\t%s\n"), moonrise);
            g_free(moonrise);
            APPEND_TEXT_ITEM_REAL(value);

            moonset = format_date(data->current_astro->moonset, NULL, TRUE);
            value = g_strdup_printf(_("\tMoonset:\t%s\n"), moonset);
            g_free(moonset);
            APPEND_TEXT_ITEM_REAL(value);
        }
    } else {
        value = g_strdup(_("\tData not available, will use sane "
                           "default values for night and day.\n"));
        APPEND_TEXT_ITEM_REAL(value);
    }

    /* temperatures */
    APPEND_BTEXT(_("\nTemperatures\n"));
    APPEND_TEXT_ITEM(_("Temperature"), TEMPERATURE);
    APPEND_TEXT_ITEM(_("Dew point"), DEWPOINT);
    APPEND_TEXT_ITEM(_("Apparent temperature"), APPARENT_TEMPERATURE);

    /* wind */
    APPEND_BTEXT(_("\nWind\n"));
    wind = get_data(conditions, data->units, WIND_SPEED,
                    FALSE, data->night_time);
    rawvalue = get_data(conditions, data->units, WIND_BEAUFORT,
                        FALSE, data->night_time);
    value = g_strdup_printf(_("\tSpeed: %s %s (%s on the Beaufort scale)\n"),
                            wind, get_unit(data->units, WIND_SPEED),
                            rawvalue);
    g_free(rawvalue);
    g_free(wind);
    APPEND_TEXT_ITEM_REAL(value);

    /* wind direction */
    rawvalue = get_data(conditions, data->units, WIND_DIRECTION_DEG,
                        FALSE, data->night_time);
    wind = get_data(conditions, data->units, WIND_DIRECTION,
                        FALSE, data->night_time);
    value = g_strdup_printf(_("\tDirection: %s (%s%s)\n"),
                            wind, rawvalue,
                            get_unit(data->units, WIND_DIRECTION_DEG));
    g_free(rawvalue);
    g_free(wind);
    APPEND_TEXT_ITEM_REAL(value);

    /* precipitation */
    APPEND_BTEXT(_("\nPrecipitation\n"));
    APPEND_TEXT_ITEM(_("Precipitation amount"), PRECIPITATION);

    /* atmosphere */
    APPEND_BTEXT(_("\nAtmosphere\n"));
    APPEND_TEXT_ITEM(_("Barometric pressure"), PRESSURE);
    APPEND_TEXT_ITEM(_("Relative humidity"), HUMIDITY);

    /* clouds */
    APPEND_BTEXT(_("\nClouds\n"));
    APPEND_TEXT_ITEM(_("Fog"), FOG);
    APPEND_TEXT_ITEM(_("Low clouds"), CLOUDS_LOW);
    APPEND_TEXT_ITEM(_("Middle clouds"), CLOUDS_MID);
    APPEND_TEXT_ITEM(_("High clouds"), CLOUDS_HIGH);
    APPEND_TEXT_ITEM(_("Cloudiness"), CLOUDINESS);

    /* credits */
    gdk_color_parse("#0000ff", &lnk_color);
    ltag_img = gtk_text_buffer_create_tag(buffer, "lnk0", "foreground-gdk",
                                          &lnk_color, NULL);
    ltag_metno = gtk_text_buffer_create_tag(buffer, "lnk1", "foreground-gdk",
                                            &lnk_color, NULL);
    ltag_wiki = gtk_text_buffer_create_tag(buffer, "lnk2", "foreground-gdk",
                                           &lnk_color, NULL);
    ltag_geonames = gtk_text_buffer_create_tag(buffer, "lnk3",
                                               "foreground-gdk",
                                               &lnk_color, NULL);
    APPEND_BTEXT(_("\nCredits\n"));
    APPEND_LINK_ITEM(_("\tEncyclopedic information partly taken from\n\t\t"),
                     _("Wikipedia"), "http://wikipedia.org", ltag_wiki);
    APPEND_LINK_ITEM(_("\n\tElevation and timezone data provided by\n\t\t"),
                     _("GeoNames"),
                     "http://geonames.org/", ltag_geonames);
    APPEND_LINK_ITEM(_("\n\tWeather and astronomical data from\n\t\t"),
                     _("The Norwegian Meteorological Institute"),
                     "http://met.no/", ltag_metno);
    g_object_set_data_full(G_OBJECT(ltag_img), "url",   /* url for image */
                           g_strdup("http://met.no"), g_free);

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
                         G_CALLBACK(icon_clicked), ltag_img);
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


static gchar *
forecast_cell_get_tooltip_text(plugin_data *data,
                               xml_time *fcdata)
{
    GString *text;
    gchar *result, *value;
    const gchar *unit;

    /* TRANSLATORS: Please use spaces as needed or desired to properly
       align the values; Monospace font is enforced with <tt> tags for
       alignment, and the text is enclosed in <small> tags because
       that looks much better and saves space.
    */
    text = g_string_new(_("<b>Times used for calculations</b>\n"));
    value = format_date(fcdata->start, NULL, TRUE);
    g_string_append_printf(text, _("<tt><small>"
                                   "Interval start:       %s"
                                   "</small></tt>\n"),
                           value);
    g_free(value);
    value = format_date(fcdata->end, NULL, TRUE);
    g_string_append_printf(text, _("<tt><small>"
                                   "Interval end:         %s"
                                   "</small></tt>\n"),
                           value);
    g_free(value);
    value = format_date(fcdata->point, NULL, TRUE);
    g_string_append_printf(text, _("<tt><small>"
                                   "Data calculated for:  %s"
                                   "</small></tt>\n\n"),
                           value);
    g_free(value);

    g_string_append(text, _("<b>Temperatures</b>\n"));
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Dew point:            %s%s%s"
                          "</small></tt>\n"),
                        DEWPOINT);
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Apparent temperature: %s%s%s"
                          "</small></tt>\n\n"),
                        APPARENT_TEMPERATURE);

    g_string_append(text, _("<b>Atmosphere</b>\n"));
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Barometric pressure:  %s%s%s"
                          "</small></tt>\n"),
                        PRESSURE);
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Relative humidity:    %s%s%s"
                          "</small></tt>\n\n"),
                        HUMIDITY);

    g_string_append(text, _("<b>Precipitation</b>\n"));
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Amount:        %s%s%s"
                          "</small></tt>\n\n"),
                        PRECIPITATION);

    g_string_append(text, _("<b>Clouds</b>\n"));
    /* TRANSLATORS: Clouds percentages are aligned to the right in the
       tooltip, the %5s are needed for that and are used both for
       rounded and unrounded values. */
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Fog:           %5s%s%s"
                          "</small></tt>\n"), FOG);
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Low clouds:    %5s%s%s"
                          "</small></tt>\n"), CLOUDS_LOW);
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Middle clouds: %5s%s%s"
                          "</small></tt>\n"), CLOUDS_MID);
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "High clouds:   %5s%s%s"
                          "</small></tt>\n"), CLOUDS_HIGH);
    APPEND_TOOLTIP_ITEM(_("<tt><small>"
                          "Cloudiness:    %5s%s%s"
                          "</small></tt>"), CLOUDINESS);

    /* Free GString only and return its character data */
    result = text->str;
    g_string_free(text, FALSE);
    return result;
}


static gchar *
forecast_day_header_tooltip_text(xml_astro *astro)
{
    GString *text;
    gchar *result, *day, *sunrise, *sunset, *moonrise, *moonset;

    /* TRANSLATORS: Please use spaces as needed or desired to properly
       align the values; Monospace font is enforced with <tt> tags for
       alignment, and the text is enclosed in <small> tags because
       that looks much better and saves space.
    */

    text = g_string_new("");
    if (astro) {
        day = format_date(astro->day, "%Y-%m-%d", TRUE);
        g_string_append_printf(text, _("<b>%s</b>\n"), day);
        g_free(day);

        if (astro->sun_never_rises)
            g_string_append(text, _("<tt><small>"
                                    "Sunrise: The sun never rises this day."
                                    "</small></tt>\n"));
        else if (astro->sun_never_sets)
            g_string_append(text, _("<tt><small>"
                                    "Sunset: The sun never sets this day."
                                    "</small></tt>\n"));
        else {
            sunrise = format_date(astro->sunrise, NULL, TRUE);
            g_string_append_printf(text, _("<tt><small>"
                                           "Sunrise: %s"
                                           "</small></tt>\n"), sunrise);
            g_free(sunrise);

            sunset = format_date(astro->sunset, NULL, TRUE);
            g_string_append_printf(text, _("<tt><small>"
                                           "Sunset:  %s"
                                           "</small></tt>\n\n"), sunset);
            g_free(sunset);
        }

        if (astro->moon_phase)
            g_string_append_printf(text, _("<tt><small>"
                                           "Moon phase: %s"
                                           "</small></tt>\n"),
                                   translate_moon_phase(astro->moon_phase));
        else
            g_string_append(text, _("<tt><small>"
                                    "Moon phase: Unknown"
                                    "</small></tt>\n"));

        if (astro->moon_never_rises)
            g_string_append(text, _("<tt><small>"
                                    "Moonrise: The moon never rises this day."
                                    "</small></tt>\n"));
        else if (astro->moon_never_sets)
            g_string_append(text,
                            _("<tt><small>"
                              "Moonset: The moon never sets this day."
                              "</small></tt>\n"));
        else {
            moonrise = format_date(astro->moonrise, NULL, TRUE);
            g_string_append_printf(text, _("<tt><small>"
                                           "Moonrise: %s"
                                           "</small></tt>\n"), moonrise);
            g_free(moonrise);

            moonset = format_date(astro->moonset, NULL, TRUE);
            g_string_append_printf(text, _("<tt><small>"
                                           "Moonset:  %s"
                                           "</small></tt>"), moonset);
            g_free(moonset);
        }
    }

    /* Free GString only and return its character data */
    result = text->str;
    g_string_free(text, FALSE);
    return result;
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
                  GArray *daydata,
                  gint day,
                  gint daytime)
{
    GtkWidget *box, *label, *image;
    GdkPixbuf *icon;
    const GdkColor black = {0, 0x0000, 0x0000, 0x0000};
    gchar *wind_speed, *wind_direction, *value, *rawvalue;
    xml_time *fcdata;

    box = gtk_vbox_new(FALSE, 0);

    fcdata = make_forecast_data(data->weatherdata, daydata, day, daytime);
    if (fcdata == NULL)
        return box;

    if (fcdata->location == NULL) {
        xml_time_free(fcdata);
        return box;
    }

    /* symbol */
    rawvalue = get_data(fcdata, data->units, SYMBOL,
                        FALSE, data->night_time);
    icon = get_icon(data->icon_theme, rawvalue, 48, (daytime == NIGHT));
    g_free(rawvalue);
    image = gtk_image_new_from_pixbuf(icon);
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(image), TRUE, TRUE, 0);
    if (G_LIKELY(icon))
        g_object_unref(G_OBJECT(icon));

    /* symbol description */
    rawvalue = get_data(fcdata, data->units, SYMBOL,
                        FALSE, data->night_time);
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
    rawvalue = get_data(fcdata, data->units, TEMPERATURE,
                        data->round, data->night_time);
    value = g_strdup_printf("%s %s", rawvalue,
                            get_unit(data->units, TEMPERATURE));
    g_free(rawvalue);
    label = gtk_label_new(value);
    if (!(day % 2))
        gtk_widget_modify_fg(GTK_WIDGET(label), GTK_STATE_NORMAL, &black);
    gtk_box_pack_start(GTK_BOX(box), GTK_WIDGET(label), TRUE, TRUE, 0);
    g_free(value);

    /* wind direction and speed */
    wind_direction = get_data(fcdata, data->units, WIND_DIRECTION,
                              FALSE, data->night_time);
    wind_speed = get_data(fcdata, data->units, WIND_SPEED,
                          data->round, data->night_time);
    value = g_strdup_printf("%s %s %s", wind_direction, wind_speed,
                            get_unit(data->units, WIND_SPEED));
    g_free(wind_speed);
    g_free(wind_direction);
    label = gtk_label_new(value);
    if (!(day % 2))
        gtk_widget_modify_fg(GTK_WIDGET(label), GTK_STATE_NORMAL, &black);
    gtk_box_pack_start(GTK_BOX(box), label, TRUE, TRUE, 0);
    g_free(value);

    gtk_widget_set_size_request(GTK_WIDGET(box), 150, -1);

    value = forecast_cell_get_tooltip_text(data, fcdata);
    gtk_widget_set_tooltip_markup(GTK_WIDGET(box), value);
    g_free(value);

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
    GArray *daydata;
    xml_astro *astro;
    gchar *dayname, *text;
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

        /* add tooltip to forecast day header */
        astro = get_astro_data_for_day(data->astrodata, i);
        text = forecast_day_header_tooltip_text(astro);
        gtk_widget_set_tooltip_markup(GTK_WIDGET(ebox), text);

        if (data->forecast_layout == FC_LAYOUT_CALENDAR)
            gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(ebox),
                                      i+1, i+2, 0, 1);
        else
            gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(ebox),
                                      0, 1, i+1, i+2);

        /* to speed up things, first get forecast data for all daytimes */
        daydata = get_point_data_for_day(data->weatherdata, i);

        /* get forecast data for each daytime */
        for (daytime = MORNING; daytime <= NIGHT; daytime++) {
            forecast_box = add_forecast_cell(data, daydata, i, daytime);
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
        g_array_free(daydata, FALSE);
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
    screen = GDK_SCREEN(gtk_widget_get_screen(GTK_WIDGET(data->iconimage)));
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


static void
cb_notebook_page_switched(GtkNotebook *notebook,
                          GtkNotebookPage *page,
                          guint page_num,
                          gpointer user_data)
{
    plugin_data *data = (plugin_data *) user_data;

    data->summary_remember_tab = page_num;
}


gboolean
update_summary_subtitle(plugin_data *data)
{
    time_t now_t;
    GTimeVal now;
    gchar *title, *date, *date_format;
    guint update_interval;
    gint64 now_ms;

    if (data->summary_update_timer) {
        g_source_remove(data->summary_update_timer);
        data->summary_update_timer = 0;
    }

    if (G_UNLIKELY(data->location_name == NULL) ||
        G_UNLIKELY(data->summary_window == NULL))
        return FALSE;

    time(&now_t);
#ifdef HAVE_UPOWER_GLIB
    if (data->upower_on_battery)
        date_format = "%Y-%m-%d %H:%M %z (%Z)";
    else
#endif
        date_format = "%Y-%m-%d %H:%M:%S %z (%Z)";
    date = format_date(now_t, date_format, TRUE);
    title = g_strdup_printf("%s\n%s", data->location_name, date);
    g_free(date);
    xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(data->summary_window),
                                    title);
    g_free(title);

    /* compute and schedule the next update */
    g_get_current_time(&now);
    now_ms = ((gint64) now.tv_sec * 1000) + ((gint64) now.tv_usec / 1000);
#ifdef HAVE_UPOWER_GLIB
    if (data->upower_on_battery)
        update_interval = 60000 - (now_ms % 60000) + 10;
    else
#endif
        update_interval = 1000 - (now_ms % 1000) + 10;
    data->summary_update_timer =
        g_timeout_add(update_interval, (GSourceFunc) update_summary_subtitle,
                      data);
    return FALSE;
}


GtkWidget *
create_summary_window(plugin_data *data)
{
    GtkWidget *window, *notebook, *vbox, *hbox, *label;
    GdkPixbuf *icon;
    xml_time *conditions;
    gchar *title, *symbol;

    conditions = get_current_conditions(data->weatherdata);
    window = xfce_titled_dialog_new_with_buttons(_("Weather Report"),
                                                 NULL,
                                                 GTK_DIALOG_NO_SEPARATOR,
                                                 GTK_STOCK_CLOSE,
                                                 GTK_RESPONSE_ACCEPT, NULL);
    if (G_LIKELY(data->location_name != NULL)) {
        title = g_strdup_printf("%s\n", data->location_name);
        xfce_titled_dialog_set_subtitle(XFCE_TITLED_DIALOG(window), title);
        g_free(title);
    }
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), vbox, TRUE, TRUE, 0);

    symbol = get_data(conditions, data->units, SYMBOL,
                      FALSE, data->night_time);
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
        gtk_widget_show_all(GTK_WIDGET(notebook));
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), data->summary_remember_tab);
        g_signal_connect(GTK_NOTEBOOK(notebook), "switch-page",
                         G_CALLBACK(cb_notebook_page_switched), data);
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
