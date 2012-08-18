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

#include <string.h>
#include <sys/stat.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#include "weather-translate.h"
#include "weather-http.h"
#include "weather-summary.h"
#include "weather-config.h"
#include "weather-icon.h"
#include "weather-scrollbox.h"
#include "weather-debug.h"

#define XFCEWEATHER_ROOT "weather"
#define UPDATE_INTERVAL 15
#define DATA_MAX_AGE (3 * 3600)
#define BORDER 8


#define DATA_AND_UNIT(var, item)                            \
    value = get_data(conditions, data->unit_system, item);  \
    unit = get_unit(data->unit_system, item);               \
    var = g_strdup_printf("%s%s%s",                         \
                          value,                            \
                          strcmp(unit, "°") ? " " : "",     \
                          unit);                            \
    g_free(value);



gboolean debug_mode = FALSE;


gboolean
check_envproxy(gchar **proxy_host,
               gint *proxy_port)
{
    gchar *env_proxy = NULL, *tmp, **split;

    env_proxy = getenv("HTTP_PROXY");
    if (!env_proxy)
        env_proxy = getenv("http_proxy");

    if (!env_proxy)
        return FALSE;

    tmp = strstr(env_proxy, "://");

    if (tmp && strlen(tmp) >= 3)
        env_proxy = tmp + 3;
    else
        return FALSE;

    /* we don't support username:password so return */
    tmp = strchr(env_proxy, '@');
    if (tmp)
        return FALSE;

    split = g_strsplit(env_proxy, ":", 2);

    if (!split[0])
        return FALSE;
    else if (!split[1]) {
        g_strfreev(split);
        return FALSE;
    }

    *proxy_host = g_strdup(split[0]);
    *proxy_port = (int) strtol(split[1], NULL, 0);

    g_strfreev(split);

    return TRUE;
}


static const gchar *
get_label_size(const xfceweather_data *data)
{
#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    /* use small label with low number of columns in deskbar mode */
    if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_DESKBAR)
        if (data->panel_size > 99)
            return "medium";
        else if (data->panel_size > 79)
            return "small";
        else
            return "x-small";
    else
#endif
        if (data->panel_size > 25)
            return "medium";
        else if (data->panel_size > 23)
            return "small";
        else
            return "x-small";
}


static gchar *
make_label(const xfceweather_data *data,
           data_types type)
{
    xml_time *conditions;
    const gchar *lbl, *txtsize, *unit;
    gchar *str, *value, *rawvalue;

    switch (type) {
    case TEMPERATURE:
        /* TRANSLATORS: Keep in sync with labeloptions in weather-config.c */
        lbl = _("T");
        break;
    case PRESSURE:
        lbl = _("P");
        break;
    case WIND_SPEED:
        lbl = _("WS");
        break;
    case WIND_BEAUFORT:
        lbl = _("WB");
        break;
    case WIND_DIRECTION:
        lbl = _("WD");
        break;
    case WIND_DIRECTION_DEG:
        lbl = _("WD");
        break;
    case HUMIDITY:
        lbl = _("H");
        break;
    case CLOUDS_LOW:
        lbl = _("CL");
        break;
    case CLOUDS_MED:
        lbl = _("CM");
        break;
    case CLOUDS_HIGH:
        lbl = _("CH");
        break;
    case CLOUDINESS:
        lbl = _("C");
        break;
    case FOG:
        lbl = _("F");
        break;
    case PRECIPITATIONS:
        lbl = _("R");
        break;
    default:
        lbl = "?";
        break;
    }

    txtsize = get_label_size(data);

    /* get current weather conditions */
    conditions = get_current_conditions(data->weatherdata);
    rawvalue = get_data(conditions, data->unit_system, type);

    switch (type) {
    case WIND_DIRECTION:
        value = translate_wind_direction(rawvalue);
        break;
    case WIND_SPEED:
        value = translate_wind_speed(rawvalue, data->unit_system);
        break;
    default:
        value = NULL;
        break;
    }

    if (data->labels->len > 1) {
        if (value != NULL) {
            str = g_strdup_printf("<span size=\"%s\">%s: %s</span>",
                                  txtsize, lbl, value);
            g_free(value);
        } else {
            unit = get_unit(data->unit_system, type);
            str = g_strdup_printf("<span size=\"%s\">%s: %s%s%s</span>",
                                  txtsize, lbl, rawvalue,
                                  strcmp(unit, "°") ? " " : "", unit);
        }
    } else {
        if (value != NULL) {
            str = g_strdup_printf("<span size=\"%s\">%s</span>",
                                  txtsize, value);
            g_free(value);
        } else {
            unit = get_unit(data->unit_system, type);
            str = g_strdup_printf("<span size=\"%s\">%s%s%s</span>",
                                  txtsize, rawvalue,
                                  strcmp(unit, "°") ? " " : "", unit);
        }
    }
    g_free(rawvalue);
    return str;
}


static void
update_icon(xfceweather_data *data)
{
    GdkPixbuf *icon = NULL;
    xml_time *conditions;
    gchar *str;
    gint size;

    size = data->size;
#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    /* make icon double-size in deskbar mode */
    if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_DESKBAR &&
        data->size != data->panel_size)
        size *= 2;
#endif

    /* set icon according to current weather conditions */
    conditions = get_current_conditions(data->weatherdata);
    str = get_data(conditions, data->unit_system, SYMBOL);
    icon = get_icon(str, size, data->night_time);
    g_free(str);
    gtk_image_set_from_pixbuf(GTK_IMAGE(data->iconimage), icon);
    if (G_LIKELY(icon))
        g_object_unref(G_OBJECT(icon));
}


static void
update_scrollbox(xfceweather_data *data)
{
    xml_time *conditions;
    guint i;
    data_types type;
    const gchar *txtsize;
    gchar *str;
    gint size;

    txtsize = get_label_size(data);
    gtk_scrollbox_clear(GTK_SCROLLBOX(data->scrollbox));
    if (data->weatherdata)
        for (i = 0; i < data->labels->len; i++) {
            type = g_array_index(data->labels, data_types, i);
            str = make_label(data, type);
            gtk_scrollbox_set_label(GTK_SCROLLBOX(data->scrollbox), -1, str);
            g_free(str);
        }
    else {
        str = g_strdup_printf("<span size=\"%s\">%s</span>", txtsize, _("No Data"));
        gtk_scrollbox_set_label(GTK_SCROLLBOX(data->scrollbox), -1, str);
        g_free(str);
    }
}


static void
update_current_conditions(xfceweather_data *data)
{
    if (G_UNLIKELY(data->weatherdata == NULL)) {
        update_icon(data);
        update_scrollbox(data);
        return;
    }

    if (data->weatherdata->current_conditions) {
        xml_time_free(data->weatherdata->current_conditions);
        data->weatherdata->current_conditions = NULL;
    }

    data->weatherdata->current_conditions =
        make_current_conditions(data->weatherdata);
    data->last_conditions_update = time(NULL);
    data->night_time = is_night_time(data->astrodata);
    update_icon(data);
    update_scrollbox(data);
}


static void
cb_astro_update(const gboolean succeed,
                gchar *result,
                const size_t len,
                gpointer user_data)
{
    xfceweather_data *data = user_data;
    xmlDoc *doc;
    xmlNode *cur_node;
    xml_astro *astro = NULL;

    if (G_LIKELY(succeed && result)) {
        if (g_utf8_validate(result, -1, NULL)) {
            /* force parsing as UTF-8, the XML encoding header may lie */
            doc = xmlReadMemory(result, strlen(result), NULL, "UTF-8", 0);
        } else
            doc = xmlParseMemory(result, strlen(result));
        g_free(result);

        if (G_LIKELY(doc)) {
            cur_node = xmlDocGetRootElement(doc);
            if (G_LIKELY(cur_node))
                astro = parse_astro(cur_node);
            xmlFreeDoc(doc);
        }
    }

    if (astro) {
        if (data->astrodata)
            xml_astro_free(data->astrodata);
        data->astrodata = astro;
        data->last_astro_update = time(NULL);
    }

    weather_dump(weather_dump_astrodata, data->astrodata);
}


static void
cb_update(const gboolean succeed,
          gchar *result,
          const size_t len,
          gpointer user_data)
{
    xfceweather_data *data = user_data;
    xmlDoc *doc;
    xmlNode *cur_node;
    xml_weather *weather = NULL;

    if (G_LIKELY(succeed && result)) {
        if (g_utf8_validate(result, -1, NULL)) {
            /* force parsing as UTF-8, the XML encoding header may lie */
            doc = xmlReadMemory(result, strlen(result), NULL, "UTF-8", 0);
        } else
            doc = xmlParseMemory(result, strlen(result));
        g_free(result);

        if (G_LIKELY(doc)) {
            cur_node = xmlDocGetRootElement(doc);
            if (cur_node)
                weather = parse_weather(cur_node);
            xmlFreeDoc(doc);
        }
    }

    if (G_LIKELY(weather)) {
        if (G_LIKELY(data->weatherdata))
            xml_weather_free(data->weatherdata);
        data->weatherdata = weather;
        data->last_data_update = time(NULL);
    }
    update_current_conditions(data);

    weather_dump(weather_dump_weatherdata, data->weatherdata);
}


static gboolean
need_astro_update(const xfceweather_data *data)
{
    time_t now_t;
    struct tm now_tm, last_tm;

    if (!data->updatetimeout || !data->last_astro_update)
        return TRUE;

    time(&now_t);
    now_tm = *localtime(&now_t);
    last_tm = *localtime(&(data->last_astro_update));
    if (now_tm.tm_mday != last_tm.tm_mday)
        return TRUE;

    return FALSE;
}


static gboolean
need_data_update(const xfceweather_data *data)
{
    time_t now_t;
    gint diff;

    if (!data->updatetimeout || !data->last_data_update)
        return TRUE;

    time(&now_t);
    diff = (gint) difftime(now_t, data->last_data_update);
    if (diff >= DATA_MAX_AGE)
        return TRUE;

    return FALSE;
}


static gboolean
need_conditions_update(const xfceweather_data *data)
{
    time_t now_t;
    struct tm now_tm, last_tm;

    if (!data->updatetimeout || !data->last_conditions_update)
        return TRUE;

    time(&now_t);
    now_tm = *localtime(&now_t);
    last_tm = *localtime(&(data->last_conditions_update));
    if (now_tm.tm_mday != last_tm.tm_mday ||
        now_tm.tm_hour != last_tm.tm_hour)
        return TRUE;

    return FALSE;
}


static gboolean
update_weatherdata(xfceweather_data *data)
{
    gchar *url;
    gboolean night_time;
    time_t now_t;
    struct tm now_tm;

    g_assert(data != NULL);
    if (G_UNLIKELY(data == NULL))
        return TRUE;

    if ((!data->lat || !data->lon) ||
        strlen(data->lat) == 0 || strlen(data->lon) == 0) {
        update_icon(data);
        update_scrollbox(data);
        return TRUE;
    }

    /* fetch astronomical data */
    if (need_astro_update(data)) {
        now_t = time(NULL);
        now_tm = *localtime(&now_t);

        /* build url */
        url = g_strdup_printf("/weatherapi/sunrise/1.0/?"
                              "lat=%s;lon=%s;date=%04d-%02d-%02d",
                              data->lat, data->lon,
                              now_tm.tm_year + 1900,
                              now_tm.tm_mon + 1,
                              now_tm.tm_mday);

        /* start receive thread */
        g_warning("getting http://api.yr.no/%s", url);
        weather_http_receive_data("api.yr.no", url, data->proxy_host,
                                  data->proxy_port, cb_astro_update, data);

        g_free(url);
    }

    /* fetch weather data */
    if (need_data_update(data)) {
        /* build url */
        url =
            g_strdup_printf("/weatherapi/locationforecastlts/1.1/?lat=%s;lon=%s",
                            data->lat, data->lon);

        /* start receive thread */
        g_warning("getting http://api.yr.no/%s", url);
        weather_http_receive_data("api.yr.no", url, data->proxy_host,
                                  data->proxy_port, cb_update, data);

        /* cleanup */
        g_free(url);
    }

    /* update current conditions, icon and labels */
    if (need_conditions_update(data))
        update_current_conditions(data);

    /* update night time status and icon */
    night_time = is_night_time(data->astrodata);
    if (data->night_time != night_time) {
        data->night_time = night_time;
        update_icon(data);
    }

    /* keep timeout running */
    return TRUE;
}


static GArray *
labels_clear(GArray *array)
{
    if (!array || array->len > 0) {
        if (array)
            g_array_free(array, TRUE);
        array = g_array_new(FALSE, TRUE, sizeof(data_types));
    }
    return array;
}


static void
xfceweather_set_visibility(xfceweather_data *data)
{
    if (data->labels->len > 0)
        gtk_widget_show(data->scrollbox);
    else
        gtk_widget_hide(data->scrollbox);
}


static void
xfceweather_read_config(XfcePanelPlugin *plugin,
                        xfceweather_data *data)
{
    XfceRc *rc;
    const gchar *value;
    gchar *file;
    gchar label[10];
    guint label_count = 0;
    gint val;

    if (!(file = xfce_panel_plugin_lookup_rc_file(plugin)))
        return;

    rc = xfce_rc_simple_open(file, TRUE);
    g_free(file);

    if (!rc)
        return;

    value = xfce_rc_read_entry(rc, "lat", NULL);
    if (value) {
        if (data->lat)
            g_free(data->lat);

        data->lat = g_strdup(value);
    }

    value = xfce_rc_read_entry(rc, "lon", NULL);
    if (value) {
        if (data->lon)
            g_free(data->lon);

        data->lon = g_strdup(value);
    }

    value = xfce_rc_read_entry(rc, "loc_name", NULL);
    if (value) {
        if (data->location_name)
            g_free(data->location_name);

        data->location_name = g_strdup(value);
    }

    data->unit_system = xfce_rc_read_int_entry(rc, "unit_system", METRIC);

    if (data->proxy_host) {
        g_free(data->proxy_host);
        data->proxy_host = NULL;
    }
    if (data->saved_proxy_host) {
        g_free(data->saved_proxy_host);
        data->saved_proxy_host = NULL;
    }
    value = xfce_rc_read_entry(rc, "proxy_host", NULL);
    if (value && *value)
        data->saved_proxy_host = g_strdup(value);

    data->saved_proxy_port = xfce_rc_read_int_entry(rc, "proxy_port", 0);

    data->proxy_fromenv = xfce_rc_read_bool_entry(rc, "proxy_fromenv", FALSE);
    if (data->proxy_fromenv)
        check_envproxy(&data->proxy_host, &data->proxy_port);
    else {
        data->proxy_host = g_strdup(data->saved_proxy_host);
        data->proxy_port = data->saved_proxy_port;
    }

    val = xfce_rc_read_int_entry(rc, "forecast_days", DEFAULT_FORECAST_DAYS);
    data->forecast_days =
        (val > 0 && val <= MAX_FORECAST_DAYS) ? val : DEFAULT_FORECAST_DAYS;

    data->animation_transitions =
        xfce_rc_read_bool_entry(rc, "animation_transitions", TRUE);
    gtk_scrollbox_set_animate(GTK_SCROLLBOX(data->scrollbox),
                              data->animation_transitions);

    data->labels = labels_clear(data->labels);
    val = 0;
    while (val != -1) {
        g_snprintf(label, 10, "label%d", label_count++);

        val = xfce_rc_read_int_entry(rc, label, -1);
        if (val >= 0)
            g_array_append_val(data->labels, val);
    }

    xfce_rc_close(rc);
}


static void
xfceweather_write_config(XfcePanelPlugin *plugin,
                         xfceweather_data *data)
{
    gchar label[10];
    guint i;
    XfceRc *rc;
    gchar *file;

    if (!(file = xfce_panel_plugin_save_location(plugin, TRUE)))
        return;

    /* get rid of old values */
    unlink(file);

    rc = xfce_rc_simple_open(file, FALSE);
    g_free(file);

    if (!rc)
        return;

    xfce_rc_write_int_entry(rc, "unit_system", data->unit_system);

    if (data->lat)
        xfce_rc_write_entry(rc, "lat", data->lat);

    if (data->lon)
        xfce_rc_write_entry(rc, "lon", data->lon);

    if (data->location_name)
        xfce_rc_write_entry(rc, "loc_name", data->location_name);

    xfce_rc_write_bool_entry(rc, "proxy_fromenv", data->proxy_fromenv);

    if (data->proxy_host) {
        xfce_rc_write_entry(rc, "proxy_host", data->proxy_host);
        xfce_rc_write_int_entry(rc, "proxy_port", data->proxy_port);
    }

    xfce_rc_write_int_entry(rc, "forecast_days", data->forecast_days);

    xfce_rc_write_bool_entry(rc, "animation_transitions",
                             data->animation_transitions);

    for (i = 0; i < data->labels->len; i++) {
        g_snprintf(label, 10, "label%d", i);
        xfce_rc_write_int_entry(rc, label,
                                (gint) g_array_index(data->labels,
                                                     data_types, i));
    }

    xfce_rc_close(rc);
}


static void
update_weatherdata_with_reset(xfceweather_data *data)
{
    if (data->updatetimeout)
        g_source_remove(data->updatetimeout);

    data->last_data_update = 0;
    data->last_conditions_update = 0;
    update_weatherdata(data);

    data->updatetimeout =
        g_timeout_add_seconds(UPDATE_INTERVAL,
                              (GSourceFunc) update_weatherdata,
                              data);
}


static void
update_config(xfceweather_data *data)
{
    update_weatherdata_with_reset(data);
}


static void
close_summary(GtkWidget *widget,
              gpointer *user_data)
{
    xfceweather_data *data = (xfceweather_data *) user_data;

    data->summary_window = NULL;
}


static void
forecast_click(GtkWidget *widget,
               gpointer user_data)
{
    xfceweather_data *data = (xfceweather_data *) user_data;

    if (data->summary_window != NULL)
        gtk_widget_destroy(data->summary_window);
    else {
        data->summary_window = create_summary_window(data);
        g_signal_connect(G_OBJECT(data->summary_window), "destroy",
                         G_CALLBACK(close_summary), data);
        gtk_widget_show_all(data->summary_window);
    }
}


static gboolean
cb_click(GtkWidget *widget,
         GdkEventButton *event,
         gpointer user_data)
{
    xfceweather_data *data = (xfceweather_data *) user_data;

    if (event->button == 1)
        forecast_click(widget, user_data);
    else if (event->button == 2)
        update_weatherdata_with_reset(data);

    return FALSE;
}


static gboolean
cb_scroll(GtkWidget *widget,
          GdkEventScroll *event,
          gpointer user_data)
{
    xfceweather_data *data = (xfceweather_data *) user_data;

    if (event->direction == GDK_SCROLL_UP ||
        event->direction == GDK_SCROLL_DOWN)
        gtk_scrollbox_next_label(GTK_SCROLLBOX(data->scrollbox));

    return FALSE;
}


static void
mi_click(GtkWidget *widget,
         gpointer user_data)
{
    xfceweather_data *data = (xfceweather_data *) user_data;

    update_weatherdata_with_reset(data);
}


static void
xfceweather_dialog_response(GtkWidget *dlg,
                            gint response,
                            xfceweather_dialog *dialog)
{
    xfceweather_data *data = (xfceweather_data *) dialog->wd;
    gboolean result;

    if (response == GTK_RESPONSE_HELP) {
        /* show help */
        result = g_spawn_command_line_async("exo-open --launch WebBrowser "
                                            PLUGIN_WEBSITE, NULL);

        if (G_UNLIKELY(result == FALSE))
            g_warning(_("Unable to open the following url: %s"),
                      PLUGIN_WEBSITE);
    } else {
        apply_options(dialog);
        weather_dump(weather_dump_plugindata, data);

        gtk_widget_destroy(dlg);
        gtk_list_store_clear(dialog->mdl_xmloption);
        g_slice_free(xfceweather_dialog, dialog);

        xfce_panel_plugin_unblock_menu(data->plugin);
        xfceweather_write_config(data->plugin, data);

        xfceweather_set_visibility(data);
    }
}


static void
xfceweather_create_options(XfcePanelPlugin *plugin,
                           xfceweather_data *data)
{
    GtkWidget *dlg, *vbox;
    xfceweather_dialog *dialog;

    xfce_panel_plugin_block_menu(plugin);

    dlg = xfce_titled_dialog_new_with_buttons(_("Weather Update"),
                                              GTK_WINDOW
                                              (gtk_widget_get_toplevel
                                               (GTK_WIDGET(plugin))),
                                              GTK_DIALOG_DESTROY_WITH_PARENT |
                                              GTK_DIALOG_NO_SEPARATOR,
                                              GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                              GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                              NULL);

    gtk_container_set_border_width(GTK_CONTAINER(dlg), 2);
    gtk_window_set_icon_name(GTK_WINDOW(dlg), "xfce4-settings");

    vbox = gtk_vbox_new(FALSE, BORDER);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER - 2);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), vbox, TRUE, TRUE, 0);

    dialog = create_config_dialog(data, vbox);

    g_signal_connect(G_OBJECT(dlg), "response",
                     G_CALLBACK(xfceweather_dialog_response), dialog);

    set_callback_config_dialog(dialog, update_config);

    gtk_widget_show(dlg);
}


static gchar *
weather_get_tooltip_text(const xfceweather_data *data)
{
    xml_time *conditions;
    struct tm *point_tm, *start_tm, *end_tm, *sunrise_tm, *sunset_tm;
    gchar *text, *sym, *symbol, *alt, *lat, *lon, *temp;
    gchar *windspeed, *windbeau, *winddir, *winddeg;
    gchar *pressure, *humidity, *precipitations;
    gchar *fog, *cloudiness, *sunval, *value;
    gchar sunrise[40], sunset[40];
    gchar point[40], interval_start[40], interval_end[40];
    const gchar *unit;

    conditions = get_current_conditions(data->weatherdata);
    if (G_UNLIKELY(conditions == NULL)) {
        text = g_strdup(_("Short-term forecast data unavailable."));
        return text;
    }

    /* times for forecast and point data */
    point_tm = localtime(&conditions->point);
    strftime(point, 40, "%X", point_tm);
    start_tm = localtime(&conditions->start);
    strftime(interval_start, 40, "%X", start_tm);
    end_tm = localtime(&conditions->end);
    strftime(interval_end, 40, "%X", end_tm);

    /* use sunrise and sunset times if available */
    if (data->astrodata)
        if (data->astrodata->sun_never_rises) {
            sunval = g_strdup(_("The sun never rises today."));
        } else if (data->astrodata->sun_never_sets) {
            sunval = g_strdup(_("The sun never sets today."));
        } else {
            sunrise_tm = localtime(&data->astrodata->sunrise);
            strftime(sunrise, 40, "%X", sunrise_tm);
            sunset_tm = localtime(&data->astrodata->sunset);
            strftime(sunset, 40, "%X", sunset_tm);
            sunval = g_strdup_printf(_("The sun rises at %s and sets at %s."),
                                     sunrise, sunset);
        }
    else
        sunval = g_strdup_printf("");

    sym = get_data(conditions, data->unit_system, SYMBOL);
    DATA_AND_UNIT(symbol, SYMBOL);
    DATA_AND_UNIT(alt, ALTITUDE);
    DATA_AND_UNIT(lat, LATITUDE);
    DATA_AND_UNIT(lon, LONGITUDE);
    DATA_AND_UNIT(temp, TEMPERATURE);
    DATA_AND_UNIT(windspeed, WIND_SPEED);
    DATA_AND_UNIT(windbeau, WIND_BEAUFORT);
    DATA_AND_UNIT(winddir, WIND_DIRECTION);
    DATA_AND_UNIT(winddeg, WIND_DIRECTION_DEG);
    DATA_AND_UNIT(pressure, PRESSURE);
    DATA_AND_UNIT(humidity, HUMIDITY);
    DATA_AND_UNIT(precipitations, PRECIPITATIONS);
    DATA_AND_UNIT(fog, FOG);
    DATA_AND_UNIT(cloudiness, CLOUDINESS);

    text = g_markup_printf_escaped
        /*
         * TRANSLATORS: Re-arrange and align at will, optionally using
         * abbreviations for labels if desired or necessary. Just take
         * into account the possible size constraints, the centered
         * vertical alignment of the icon - which unfortunately cannot
         * be changed easily - and try to make it compact and look
         * good!
         */
        (_("<b><span size=\"large\">%s</span></b> "
           "<span size=\"medium\">(%s)</span>\n"
           "<b><span size=\"large\">%s</span></b>\n"
           "<span size=\"smaller\">"
           "from %s to %s, with %s precipitations</span>\n\n"
           "<b>Temperature:</b> %s\t\t"
           "<span size=\"smaller\">(values at %s)</span>\n"
           "<b>Wind:</b> %s (%son the Beaufort scale) from %s(%s)\n"
           "<b>Pressure:</b> %s    <b>Humidity:</b> %s\n"
           "<b>Fog:</b> %s    <b>Cloudiness:</b> %s\n\n"
           "<span size=\"smaller\">%s</span>"),
         data->location_name,
         alt,
         translate_desc(sym, data->night_time),
         interval_start, interval_end,
         precipitations,
         temp, point,
         windspeed, windbeau, winddir, winddeg,
         pressure, humidity,
         fog, cloudiness,
         sunval);
    g_free(sunval);
    g_free(sym);
    g_free(symbol);
    g_free(alt);
    g_free(lat);
    g_free(lon);
    g_free(temp);
    g_free(windspeed);
    g_free(windbeau);
    g_free(winddir);
    g_free(winddeg);
    g_free(pressure);
    g_free(humidity);
    g_free(precipitations);
    g_free(fog);
    g_free(cloudiness);
    return text;
}


static gboolean
weather_get_tooltip_cb(GtkWidget *widget,
                       gint x,
                       gint y,
                       gboolean keyboard_mode,
                       GtkTooltip *tooltip,
                       xfceweather_data *data)
{
    GdkPixbuf *icon;
    xml_time *conditions;
    gchar *markup_text, *rawvalue;

    if (data->weatherdata == NULL)
        gtk_tooltip_set_text(tooltip, _("Cannot update weather data"));
    else {
        markup_text = weather_get_tooltip_text(data);
        gtk_tooltip_set_markup(tooltip, markup_text);
        g_free(markup_text);
    }

    conditions = get_current_conditions(data->weatherdata);
    rawvalue = get_data(conditions, data->unit_system, SYMBOL);
    icon = get_icon(rawvalue, 128, data->night_time);
    g_free(rawvalue);
    gtk_tooltip_set_icon(tooltip, icon);
    g_object_unref(G_OBJECT(icon));

    return TRUE;
}


static xfceweather_data *
xfceweather_create_control(XfcePanelPlugin *plugin)
{
    xfceweather_data *data = g_slice_new0(xfceweather_data);
    GtkWidget *refresh, *mi;
    data_types lbl;
    GdkPixbuf *icon = NULL;

    /* Initialize with sane default values */
    data->plugin = plugin;
    data->lat = NULL;
    data->lon = NULL;
    data->location_name = NULL;
    data->unit_system = METRIC;
    data->weatherdata = NULL;
    data->proxy_host = NULL;
    data->proxy_port = 0;
    data->saved_proxy_host = NULL;
    data->saved_proxy_port = 0;
    data->animation_transitions = FALSE;
    data->forecast_days = DEFAULT_FORECAST_DAYS;

    data->scrollbox = gtk_scrollbox_new();

    data->size = xfce_panel_plugin_get_size(plugin);
    icon = get_icon(NULL, 16, FALSE);
    data->iconimage = gtk_image_new_from_pixbuf(icon);

    if (G_LIKELY(icon))
        g_object_unref(G_OBJECT(icon));

    data->labels = g_array_new(FALSE, TRUE, sizeof(data_types));

    data->vbox_center_scrollbox = gtk_vbox_new(FALSE, 0);
    data->top_hbox = gtk_hbox_new(FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(data->iconimage), 1, 0.5);
    gtk_box_pack_start(GTK_BOX(data->top_hbox),
                       data->iconimage, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->vbox_center_scrollbox),
                       data->scrollbox, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->top_hbox),
                       data->vbox_center_scrollbox, TRUE, FALSE, 0);

    data->top_vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->top_vbox),
                       data->top_hbox, TRUE, FALSE, 0);

    data->tooltipbox = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(data->tooltipbox), data->top_vbox);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(data->tooltipbox), FALSE);
    gtk_widget_show_all(data->tooltipbox);

    g_object_set(G_OBJECT(data->tooltipbox), "has-tooltip", TRUE, NULL);
    g_signal_connect(G_OBJECT(data->tooltipbox), "query-tooltip",
                     G_CALLBACK(weather_get_tooltip_cb),
                     data);
    xfce_panel_plugin_add_action_widget(plugin, data->tooltipbox);

    g_signal_connect(G_OBJECT(data->tooltipbox), "button-press-event",
                     G_CALLBACK(cb_click), data);
    g_signal_connect(G_OBJECT(data->tooltipbox), "scroll-event",
                     G_CALLBACK(cb_scroll), data);
    gtk_widget_add_events(data->scrollbox, GDK_BUTTON_PRESS_MASK);

    /* add refresh button to right click menu, for people who missed
       the middle mouse click feature */
    refresh = gtk_image_menu_item_new_from_stock("gtk-refresh", NULL);
    gtk_widget_show(refresh);

    g_signal_connect(G_OBJECT(refresh), "activate",
                     G_CALLBACK(mi_click), data);

    xfce_panel_plugin_menu_insert_item(plugin, GTK_MENU_ITEM(refresh));

    /* add forecast window to right click menu, for people who missed
       the left mouse click feature */
    mi = gtk_image_menu_item_new_with_mnemonic(_("_Forecast"));
    icon = get_icon("SUN", 16, FALSE);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),
                                  gtk_image_new_from_pixbuf(icon));
    if (G_LIKELY(icon))
        g_object_unref(G_OBJECT(icon));
    gtk_widget_show(mi);

    g_signal_connect(G_OBJECT(mi), "activate",
                     G_CALLBACK(forecast_click), data);

    xfce_panel_plugin_menu_insert_item(plugin, GTK_MENU_ITEM(mi));

    /* assign to tempval because g_array_append_val() is using & operator */
    lbl = TEMPERATURE;
    g_array_append_val(data->labels, lbl);
    lbl = WIND_DIRECTION;
    g_array_append_val(data->labels, lbl);
    lbl = WIND_SPEED;
    g_array_append_val(data->labels, lbl);

    /*
     * FIXME: Without this the first label looks odd, because
     * the gc isn't created yet
     */
    gtk_scrollbox_set_label(GTK_SCROLLBOX(data->scrollbox), -1, "1");
    gtk_scrollbox_clear(GTK_SCROLLBOX(data->scrollbox));

    data->updatetimeout =
        g_timeout_add_seconds(UPDATE_INTERVAL,
                              (GSourceFunc) update_weatherdata,
                              data);

    return data;
}


static void
xfceweather_free(XfcePanelPlugin *plugin,
                 xfceweather_data *data)
{
    g_assert(data != NULL);
    weather_http_cleanup_queue();

    if (data->weatherdata)
        xml_weather_free(data->weatherdata);

    if (data->astrodata)
        xml_astro_free(data->astrodata);

    if (data->updatetimeout) {
        g_source_remove(data->updatetimeout);
        data->updatetimeout = 0;
    }

    xmlCleanupParser();

    /* free chars */
    g_free(data->lat);
    g_free(data->lon);
    g_free(data->location_name);
    g_free(data->proxy_host);
    g_free(data->saved_proxy_host);

    /* free array */
    g_array_free(data->labels, TRUE);

    g_slice_free(xfceweather_data, data);
    data = NULL;
}


static gboolean
xfceweather_set_size(XfcePanelPlugin *panel,
                     gint size,
                     xfceweather_data *data)
{
    data->panel_size = size;
#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    size /= xfce_panel_plugin_get_nrows(panel);
#endif
    data->size = size;

    update_icon(data);
    update_scrollbox(data);

    weather_dump(weather_dump_plugindata, data);

    /* we handled the size */
    return TRUE;
}


#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
static gboolean
xfceweather_set_mode(XfcePanelPlugin *panel,
                     XfcePanelPluginMode mode,
                     xfceweather_data *data)
{
    GtkWidget *parent = gtk_widget_get_parent(data->vbox_center_scrollbox);

    data->panel_orientation = xfce_panel_plugin_get_mode(panel);
    data->orientation = (mode != XFCE_PANEL_PLUGIN_MODE_VERTICAL)
        ? GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;

    g_object_ref(G_OBJECT(data->vbox_center_scrollbox));
    gtk_container_remove(GTK_CONTAINER(parent), data->vbox_center_scrollbox);

    if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
        gtk_box_pack_start(GTK_BOX(data->top_hbox),
                           data->vbox_center_scrollbox, TRUE, FALSE, 0);
    else
        gtk_box_pack_start(GTK_BOX(data->top_vbox),
                           data->vbox_center_scrollbox, TRUE, FALSE, 0);
    g_object_unref(G_OBJECT(data->vbox_center_scrollbox));

    if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_DESKBAR)
        xfce_panel_plugin_set_small(XFCE_PANEL_PLUGIN(panel), FALSE);
    else
        xfce_panel_plugin_set_small(XFCE_PANEL_PLUGIN(panel), TRUE);

    gtk_scrollbox_set_orientation(GTK_SCROLLBOX(data->scrollbox),
                                  data->orientation);

    update_icon(data);
    update_scrollbox(data);

    weather_dump(weather_dump_plugindata, data);

    /* we handled the orientation */
    return TRUE;
}


#else


static gboolean
xfceweather_set_orientation(XfcePanelPlugin *panel,
                            GtkOrientation orientation,
                            xfceweather_data *data)
{
    GtkWidget *parent = gtk_widget_get_parent(data->vbox_center_scrollbox);

    data->orientation = GTK_ORIENTATION_HORIZONTAL;
    data->panel_orientation = orientation;

    g_object_ref(G_OBJECT(data->vbox_center_scrollbox));
    gtk_container_remove(GTK_CONTAINER(parent), data->vbox_center_scrollbox);

    if (data->panel_orientation == GTK_ORIENTATION_HORIZONTAL)
        gtk_box_pack_start(GTK_BOX(data->top_hbox),
                           data->vbox_center_scrollbox, TRUE, FALSE, 0);
    else
        gtk_box_pack_start(GTK_BOX(data->top_vbox),
                           data->vbox_center_scrollbox, TRUE, FALSE, 0);
    g_object_unref(G_OBJECT(data->vbox_center_scrollbox));

    gtk_scrollbox_set_orientation(GTK_SCROLLBOX(data->scrollbox),
                                  data->panel_orientation);

    update_icon(data);
    update_scrollbox(data);

    weather_dump(weather_dump_plugindata, data);

    /* we handled the orientation */
    return TRUE;
}
#endif


static void
weather_construct(XfcePanelPlugin *plugin)
{
    xfceweather_data *data;
    const gchar *panel_debug_env;

    /* Enable debug level logging if PANEL_DEBUG contains G_LOG_DOMAIN */
    panel_debug_env = g_getenv("PANEL_DEBUG");
    if (panel_debug_env && strstr(panel_debug_env, G_LOG_DOMAIN))
        debug_mode = TRUE;
    weather_debug_init(G_LOG_DOMAIN, debug_mode);
    weather_debug("weather plugin version " VERSION " starting up");

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
    data = xfceweather_create_control(plugin);
    xfceweather_read_config(plugin, data);
    xfceweather_set_visibility(data);

#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    xfceweather_set_mode(plugin, xfce_panel_plugin_get_mode(plugin), data);
#else
    xfceweather_set_orientation(plugin, xfce_panel_plugin_get_orientation(plugin), data);
#endif
    xfceweather_set_size(plugin, xfce_panel_plugin_get_size(plugin), data);

    gtk_container_add(GTK_CONTAINER(plugin), data->tooltipbox);

    g_signal_connect(G_OBJECT(plugin), "free-data",
                     G_CALLBACK(xfceweather_free), data);
    g_signal_connect(G_OBJECT(plugin), "save",
                     G_CALLBACK(xfceweather_write_config), data);
    g_signal_connect(G_OBJECT(plugin), "size-changed",
                     G_CALLBACK(xfceweather_set_size), data);
#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    g_signal_connect(G_OBJECT(plugin), "mode-changed",
                     G_CALLBACK(xfceweather_set_mode), data);
#else
    g_signal_connect(G_OBJECT(plugin), "orientation-changed",
                     G_CALLBACK(xfceweather_set_orientation), data);
#endif
    xfce_panel_plugin_menu_show_configure(plugin);
    g_signal_connect(G_OBJECT(plugin), "configure-plugin",
                     G_CALLBACK(xfceweather_create_options), data);

    weather_dump(weather_dump_plugindata, data);

    update_weatherdata(data);
}

XFCE_PANEL_PLUGIN_REGISTER(weather_construct);
