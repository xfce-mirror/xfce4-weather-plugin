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
#include <sys/stat.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#include "weather-translate.h"
#include "weather-summary.h"
#include "weather-config.h"
#include "weather-icon.h"
#include "weather-scrollbox.h"
#include "weather-debug.h"

#define XFCEWEATHER_ROOT "weather"
#define CACHE_FILE_MAX_AGE (48 * 3600)
#define BORDER (8)
#define CONN_TIMEOUT (10)        /* connection timeout in seconds */
#define CONN_MAX_ATTEMPTS (3)    /* max retry attempts using small interval */
#define CONN_RETRY_INTERVAL_SMALL (10)
#define CONN_RETRY_INTERVAL_LARGE (10 * 60)

/* met.no sunrise API returns data for up to 30 days in the future and
   will return an error page if too many days are requested. Let's
   play it safe and request fewer than that, since we can only get a
   10 days forecast too. */
#define ASTRODATA_MAX_DAYS 25

/* power saving update interval in seconds used as a precaution to
   deal with suspend/resume events etc., when nothing needs to be
   updated earlier: */
#define POWERSAVE_UPDATE_INTERVAL (30)

/* standard update interval in seconds used as a precaution to deal
   with suspend/resume events etc., when nothing needs to be updated
   earlier: */
#define UPDATE_INTERVAL (10)

#define DATA_AND_UNIT(var, item)                        \
    value = get_data(conditions, data->units, item,     \
                     data->round, data->night_time);    \
    unit = get_unit(data->units, item);                 \
    var = g_strdup_printf("%s%s%s",                     \
                          value,                        \
                          strcmp(unit, "°") ? " " : "", \
                          unit);                        \
    g_free(value);

#define CACHE_APPEND(str, val)                  \
    if (val)                                    \
        g_string_append_printf(out, str, val);

#define CACHE_FREE_VARS()                       \
    g_free(locname);                            \
    g_free(lat);                                \
    g_free(lon);                                \
    if (keyfile)                                \
        g_key_file_free(keyfile);

#define CACHE_READ_STRING(var, key)                         \
    var = g_key_file_get_string(keyfile, group, key, NULL); \

#define SCHEDULE_WAKEUP_COMPARE(var, reason)        \
    if (difftime(var, now_t) < diff) {              \
        data->next_wakeup = var;                    \
        diff = difftime(data->next_wakeup, now_t);  \
        data->next_wakeup_reason = reason;          \
    }


gboolean debug_mode = FALSE;


static void write_cache_file(plugin_data *data);

static void schedule_next_wakeup(plugin_data *data);


void
weather_http_queue_request(SoupSession *session,
                           const gchar *uri,
                           SoupSessionCallback callback_func,
                           gpointer user_data)
{
    SoupMessage *msg;

    msg = soup_message_new("GET", uri);
    soup_session_queue_message(session, msg, callback_func, user_data);
}


static gchar *
make_label(const plugin_data *data,
           data_types type)
{
    xml_time *conditions;
    const gchar *lbl, *unit;
    gchar *str, *value;

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
    case DEWPOINT:
        lbl = _("D");
        break;
    case APPARENT_TEMPERATURE:
        lbl = _("A");
        break;
    case CLOUDS_LOW:
        lbl = _("CL");
        break;
    case CLOUDS_MID:
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
    case PRECIPITATION:
        lbl = _("R");
        break;
    default:
        lbl = "?";
        break;
    }

    /* get current weather conditions */
    conditions = get_current_conditions(data->weatherdata);
    unit = get_unit(data->units, type);
    value = get_data(conditions, data->units, type,
                     data->round, data->night_time);

    if (data->labels->len > 1)
        str = g_strdup_printf("%s: %s%s%s", lbl, value,
                              strcmp(unit, "°") || strcmp(unit, "")
                              ? " " : "", unit);
    else
        str = g_strdup_printf("%s%s%s", value,
                              strcmp(unit, "°") || strcmp(unit, "")
                              ? " " : "", unit);
    g_free(value);
    return str;
}


static update_info *
make_update_info(const guint check_interval)
{
    update_info *upi;

    upi = g_slice_new0(update_info);
    if (G_UNLIKELY(upi == NULL))
        return NULL;

    memset(&upi->last, 0, sizeof(upi->last));
    upi->next = time(NULL);
    upi->check_interval = check_interval;
    return upi;
}


static void
init_update_infos(plugin_data *data)
{
    if (G_LIKELY(data->astro_update))
        g_slice_free(update_info, data->astro_update);
    if (G_LIKELY(data->weather_update))
        g_slice_free(update_info, data->weather_update);
    if (G_LIKELY(data->conditions_update))
        g_slice_free(update_info, data->conditions_update);

    data->astro_update = make_update_info(24 * 3600);
    data->weather_update = make_update_info(60 * 60);
    data->conditions_update = make_update_info(5 * 60);
}


/*
 * Return the weather plugin cache directory, creating it if
 * necessary. The string returned does not contain a trailing slash.
 */
gchar *
get_cache_directory(void)
{
    gchar *dir = g_strconcat(g_get_user_cache_dir(), G_DIR_SEPARATOR_S,
                             "xfce4", G_DIR_SEPARATOR_S, "weather",
                             NULL);
    g_mkdir_with_parents(dir, 0755);
    return dir;
}


static gint
get_tooltip_icon_size(plugin_data *data)
{
    switch (data->tooltip_style) {
    case TOOLTIP_SIMPLE:
        return 96;
    case TOOLTIP_VERBOSE:
    default:
        return 128;
    }
}


void
update_timezone(plugin_data *data)
{
    if (data->timezone && strlen(data->timezone) > 0)
        g_setenv("TZ", data->timezone, TRUE);
    else {
        if (data->timezone_initial && strlen(data->timezone_initial) > 0)
            g_setenv("TZ", data->timezone_initial, TRUE);
        else
            g_unsetenv("TZ");
    }
}


void
update_icon(plugin_data *data)
{
    GdkPixbuf *icon;
    xml_time *conditions;
    gchar *str;
    gint size;

    size = data->panel_size;
#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    /* make icon smaller when not single-row in multi-row panels */
    if (!data->single_row && data->panel_rows > 2)
        size *= 0.80;
#endif
    /* take into account the border of the toggle button */
    size -= 2;

    /* set panel icon according to current weather conditions */
    conditions = get_current_conditions(data->weatherdata);
    str = get_data(conditions, data->units, SYMBOL,
                   data->round, data->night_time);
    icon = get_icon(data->icon_theme, str, size, data->night_time);
    gtk_image_set_from_pixbuf(GTK_IMAGE(data->iconimage), icon);
    if (G_LIKELY(icon))
        g_object_unref(G_OBJECT(icon));

    /* set tooltip icon too */
    size = get_tooltip_icon_size(data);
    if (G_LIKELY(data->tooltip_icon))
        g_object_unref(G_OBJECT(data->tooltip_icon));
    data->tooltip_icon = get_icon(data->icon_theme, str, size, data->night_time);
    g_free(str);
    weather_debug("Updated panel and tooltip icons.");
}


void
scrollbox_set_visible(plugin_data *data)
{
    if (data->show_scrollbox && data->labels->len > 0)
        gtk_widget_show_all(GTK_WIDGET(data->vbox_center_scrollbox));
    else
        gtk_widget_hide_all(GTK_WIDGET(data->vbox_center_scrollbox));
    gtk_scrollbox_set_visible(GTK_SCROLLBOX(data->scrollbox),
                              data->show_scrollbox);
}


void
update_scrollbox(plugin_data *data,
                 gboolean immediately)
{
    GString *out;
    gchar *label = NULL;
    data_types type;
    gint i = 0, j = 0;

    gtk_scrollbox_clear_new(GTK_SCROLLBOX(data->scrollbox));
    if (data->weatherdata && data->weatherdata->current_conditions) {
        while (i < data->labels->len) {
            j = 0;
            out = g_string_sized_new(128);
            while ((i + j) < data->labels->len && j < data->scrollbox_lines) {
                type = g_array_index(data->labels, data_types, i + j);
                label = make_label(data, type);
                g_string_append_printf(out, "%s%s", label,
                                       (j < (data->scrollbox_lines - 1) &&
                                        (i + j + 1) < data->labels->len
                                        ? "\n"
                                        : ""));
                g_free(label);
                j++;
            }
            gtk_scrollbox_add_label(GTK_SCROLLBOX(data->scrollbox),
                                    -1, out->str);
            g_string_free(out, TRUE);
            i = i + j;
        }
        weather_debug("Added %u labels to scrollbox.", data->labels->len);
    } else {
        gtk_scrollbox_add_label(GTK_SCROLLBOX(data->scrollbox), -1,
                                _("No Data"));
        weather_debug("No weather data available, set single label '%s'.",
                      _("No Data"));
    }
#ifdef HAVE_UPOWER_GLIB
    if (data->upower_on_battery)
        gtk_scrollbox_set_animate(GTK_SCROLLBOX(data->scrollbox), FALSE);
    else
#endif
        gtk_scrollbox_set_animate(GTK_SCROLLBOX(data->scrollbox),
                                  data->scrollbox_animate);
    /* update labels immediately (mainly used on config change) */
    if (immediately) {
        gtk_scrollbox_prev_label(GTK_SCROLLBOX(data->scrollbox));
        gtk_scrollbox_swap_labels(GTK_SCROLLBOX(data->scrollbox));
    }
    scrollbox_set_visible(data);
    weather_debug("Updated scrollbox.");
}


/* get astrodata for the current day */
static void
update_current_astrodata(plugin_data *data)
{
    time_t now_t = time(NULL);
    gdouble tdiff = -99999;

    if (G_UNLIKELY(data->astrodata == NULL)) {
        data->current_astro = NULL;
        return;
    }

    if (data->current_astro)
        tdiff = difftime(now_t, data->current_astro->day);

    if (data->current_astro == NULL || tdiff >= 24 * 3600 || tdiff < 0) {
        data->current_astro = get_astro_data_for_day(data->astrodata, 0);
        if (G_UNLIKELY(data->current_astro == NULL))
            weather_debug("No current astrodata available.");
        else
            weather_debug("Updated current astrodata.");
    }
}


static void
update_current_conditions(plugin_data *data,
                          gboolean immediately)
{
    struct tm now_tm;

    if (G_UNLIKELY(data->weatherdata == NULL)) {
        update_icon(data);
        update_scrollbox(data, TRUE);
        schedule_next_wakeup(data);
        return;
    }

    if (data->weatherdata->current_conditions) {
        xml_time_free(data->weatherdata->current_conditions);
        data->weatherdata->current_conditions = NULL;
    }
    /* use exact 5 minute intervals for calculation */
    time(&data->conditions_update->last);
    now_tm = *localtime(&data->conditions_update->last);
    now_tm.tm_min -= (now_tm.tm_min % 5);
    if (now_tm.tm_min < 0)
        now_tm.tm_min = 0;
    now_tm.tm_sec = 0;
    data->conditions_update->last = mktime(&now_tm);

    data->weatherdata->current_conditions =
        make_current_conditions(data->weatherdata,
                                data->conditions_update->last);

    /* update current astrodata */
    update_current_astrodata(data);
    data->night_time = is_night_time(data->current_astro);

    /* update widgets */
    update_icon(data);
    update_scrollbox(data, immediately);

    /* schedule next update */
    now_tm.tm_min += 5;
    data->conditions_update->next = mktime(&now_tm);
    schedule_next_wakeup(data);

    weather_debug("Updated current conditions.");
}


static time_t
calc_next_download_time(const update_info *upi,
                        time_t retry_t) {
    struct tm retry_tm;
    guint interval;

    retry_tm = *localtime(&retry_t);

    /* If the download failed, retry immediately using a small retry
     * interval for a limited number of times. If it still fails after
     * that, continue using a larger interval or the default check,
     * whatever is smaller.
     */
    if (G_LIKELY(upi->attempt == 0))
        interval = upi->check_interval;
    else if (upi->attempt <= CONN_MAX_ATTEMPTS)
        interval = CONN_RETRY_INTERVAL_SMALL;
    else {
        if (upi->check_interval > CONN_RETRY_INTERVAL_LARGE)
            interval = CONN_RETRY_INTERVAL_LARGE;
        else
            interval = upi->check_interval;
    }

    return time_calc(retry_tm, 0, 0, 0, 0, 0, interval);
}


/*
 * Process downloaded astro data and schedule next astro update.
 */
static void
cb_astro_update(SoupSession *session,
                SoupMessage *msg,
                gpointer user_data)
{
    plugin_data *data = user_data;
    xmlDoc *doc;
    xmlNode *root_node;
    time_t now_t;
    gboolean parsing_error = TRUE;

    time(&now_t);
    data->astro_update->attempt++;
    data->astro_update->http_status_code = msg->status_code;
    if ((msg->status_code == 200 || msg->status_code == 203)) {
        doc = get_xml_document(msg);
        if (G_LIKELY(doc)) {
            root_node = xmlDocGetRootElement(doc);
            if (G_LIKELY(root_node))
                if (parse_astrodata(root_node, data->astrodata)) {
                    /* schedule next update */
                    data->astro_update->attempt = 0;
                    data->astro_update->last = now_t;
                    parsing_error = FALSE;
                }
            xmlFreeDoc(doc);
        }
        if (parsing_error)
            g_warning(_("Error parsing astronomical data!"));
    } else
        g_warning(_("Download of astronomical data failed with "
                    "HTTP Status Code %d, Reason phrase: %s"),
                  msg->status_code, msg->reason_phrase);
    data->astro_update->next = calc_next_download_time(data->astro_update,
                                                       now_t);

    astrodata_clean(data->astrodata);
    g_array_sort(data->astrodata, (GCompareFunc) xml_astro_compare);
    update_current_astrodata(data);
    if (! parsing_error)
        weather_dump(weather_dump_astrodata, data->astrodata);

    /* update icon */
    data->night_time = is_night_time(data->current_astro);
    update_icon(data);

    data->astro_update->finished = TRUE;
}


/*
 * Process downloaded weather data and schedule next weather update.
 */
static void
cb_weather_update(SoupSession *session,
                  SoupMessage *msg,
                  gpointer user_data)
{
    plugin_data *data = user_data;
    xmlDoc *doc;
    xmlNode *root_node;
    time_t now_t;
    gboolean parsing_error = TRUE;

    weather_debug("Processing downloaded weather data.");
    time(&now_t);
    data->weather_update->attempt++;
    data->weather_update->http_status_code = msg->status_code;
    if (msg->status_code == 200 || msg->status_code == 203) {
        doc = get_xml_document(msg);
        if (G_LIKELY(doc)) {
            root_node = xmlDocGetRootElement(doc);
            if (G_LIKELY(root_node))
                if (parse_weather(root_node, data->weatherdata)) {
                    data->weather_update->attempt = 0;
                    data->weather_update->last = now_t;
                    parsing_error = FALSE;
                }
            xmlFreeDoc(doc);
        }
        if (parsing_error)
            g_warning(_("Error parsing weather data!"));
    } else
        g_warning
            (_("Download of weather data failed with HTTP Status Code %d, "
               "Reason phrase: %s"), msg->status_code, msg->reason_phrase);
    data->weather_update->next = calc_next_download_time(data->weather_update,
                                                         now_t);

    xml_weather_clean(data->weatherdata);
    g_array_sort(data->weatherdata->timeslices,
                 (GCompareFunc) xml_time_compare);
    weather_debug("Updating current conditions.");
    update_current_conditions(data, !parsing_error);
    gtk_scrollbox_reset(GTK_SCROLLBOX(data->scrollbox));

    data->weather_update->finished = TRUE;
    weather_dump(weather_dump_weatherdata, data->weatherdata);
}


static gboolean
update_handler(plugin_data *data)
{
    gchar *url;
    gboolean night_time;
    time_t now_t, end_t;
    struct tm now_tm, end_tm;

    g_assert(data != NULL);
    if (G_UNLIKELY(data == NULL))
        return FALSE;

    /* plugin has not been configured yet, so simply update icon and
       scrollbox and return */
    if (G_UNLIKELY(data->lat == NULL || data->lon == NULL)) {
        update_icon(data);
        update_scrollbox(data, TRUE);
        return FALSE;
    }

    now_t = time(NULL);
    now_tm = *localtime(&now_t);

    /* check if all started downloads are finished and the cache file
       can be written */
    if (data->astro_update->started && data->astro_update->finished &&
        data->weather_update->started && data->weather_update->finished) {
        data->astro_update->started = FALSE;
        data->astro_update->finished = FALSE;
        data->weather_update->started = FALSE;
        data->weather_update->finished = FALSE;
        write_cache_file(data);
    }

    /* fetch astronomical data */
    if (difftime(data->astro_update->next, now_t) <= 0) {
        /* real next update time will be calculated when update is finished,
           this is to prevent spawning multiple updates in a row */
        data->astro_update->next = time_calc_hour(now_tm, 1);
        data->astro_update->started = TRUE;

        /* calculate date range for request */
        end_t = time_calc_day(now_tm, ASTRODATA_MAX_DAYS);
        end_tm = *localtime(&end_t);

        /* build url */
        url = g_strdup_printf("https://api.met.no/weatherapi/sunrise/1.1/?"
                              "lat=%s;lon=%s;"
                              "from=%04d-%02d-%02d;"
                              "to=%04d-%02d-%02d",
                              data->lat, data->lon,
                              now_tm.tm_year + 1900,
                              now_tm.tm_mon + 1,
                              now_tm.tm_mday,
                              end_tm.tm_year + 1900,
                              end_tm.tm_mon + 1,
                              end_tm.tm_mday);

        /* start receive thread */
        g_message(_("getting %s"), url);
        weather_http_queue_request(data->session, url, cb_astro_update, data);
        g_free(url);
    }

    /* fetch weather data */
    if (difftime(data->weather_update->next, now_t) <= 0) {
        /* real next update time will be calculated when update is finished,
           this is to prevent spawning multiple updates in a row */
        data->weather_update->next = time_calc_hour(now_tm, 1);
        data->weather_update->started = TRUE;

        /* build url */
        url =
            g_strdup_printf("https://api.met.no/weatherapi"
                            "/locationforecastlts/1.3/?lat=%s;lon=%s;msl=%d",
                            data->lat, data->lon, data->msl);

        /* start receive thread */
        g_message(_("getting %s"), url);
        weather_http_queue_request(data->session, url,
                                   cb_weather_update, data);
        g_free(url);

        /* cb_weather_update will deal with everything that follows this
         * block, so let's return instead of doing things twice */
        return FALSE;
    }

    /* update current conditions, icon and labels */
    if (difftime(data->conditions_update->next, now_t) <= 0) {
        /* real next update time will be calculated when update is finished,
           this is to prevent spawning multiple updates in a row */
        data->conditions_update->next = time_calc_hour(now_tm, 1);
        weather_debug("Updating current conditions.");
        update_current_conditions(data, FALSE);
        /* update_current_conditions updates day/night time status
           too, so quit here */
        return FALSE;
    }

    /* update night time status and icon */
    update_current_astrodata(data);
    night_time = is_night_time(data->current_astro);
    if (data->night_time != night_time) {
        weather_debug("Night time status changed, updating icon.");
        data->night_time = night_time;
        update_icon(data);
    }

    schedule_next_wakeup(data);
    return FALSE;
}


static void
schedule_next_wakeup(plugin_data *data)
{
    time_t now_t = time(NULL), next_day_t;
    gdouble diff;
    gchar *date;
    GSource *source;

    if (data->update_timer) {
        source = g_main_context_find_source_by_id(NULL, data->update_timer);
        if (source) {
            g_source_destroy(source);
            data->update_timer = 0;
        }
    }

    next_day_t = day_at_midnight(now_t, 1);
    diff = difftime(next_day_t, now_t);
	data->next_wakeup_reason = "current astro data update";
    SCHEDULE_WAKEUP_COMPARE(data->astro_update->next,
                            "astro data download");
    SCHEDULE_WAKEUP_COMPARE(data->weather_update->next,
                            "weather data download");
    SCHEDULE_WAKEUP_COMPARE(data->conditions_update->next,
                            "current conditions update");

    /* If astronomical data is unavailable, current conditions update
       will usually handle night/day. */
    if (data->current_astro) {
        if (data->night_time &&
            difftime(data->current_astro->sunrise, now_t) >= 0)
            SCHEDULE_WAKEUP_COMPARE(data->current_astro->sunrise,
                                    "sunrise icon change");
        if (!data->night_time &&
            difftime(data->current_astro->sunset, now_t) >= 0)
            SCHEDULE_WAKEUP_COMPARE(data->current_astro->sunset,
                                    "sunset icon change");
    }

#ifdef HAVE_UPOWER_GLIB
    if (data->upower_on_battery && diff > POWERSAVE_UPDATE_INTERVAL) {
        /* next wakeup time is greater than the power saving check
           interval, so call the update handler earlier to deal with
           cases like system resume events etc. */
        diff = POWERSAVE_UPDATE_INTERVAL;
        data->next_wakeup_reason = "regular check (power saving)";
    } else
#endif
    if (diff > UPDATE_INTERVAL) {
        /* next wakeup time is greater than the standard check
           interval, so call the update handler earlier to deal with
           cases like system resume events etc. */
        diff = UPDATE_INTERVAL;
        data->next_wakeup_reason = "regular check";
    } else if (diff < 0) {
        /* last wakeup time expired, force update immediately */
        diff = 0;
        data->next_wakeup_reason = "forced";
    }

    date = format_date(now_t, "%Y-%m-%d %H:%M:%S", TRUE);
    data->update_timer =
        g_timeout_add_seconds((guint) diff,
                              (GSourceFunc) update_handler, data);
    if (!strcmp(data->next_wakeup_reason, "regular check"))
        weather_debug("[%s]: Running regular check for updates, "
                      "interval %d secs.", date, UPDATE_INTERVAL);
    else {
        weather_dump(weather_dump_plugindata, data);
        weather_debug("[%s]: Next wakeup in %.0f seconds, reason: %s",
                      date, diff, data->next_wakeup_reason);
    }
    g_free(date);
}


GArray *
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
constrain_to_limits(gint *i,
                    const gint min,
                    const gint max)
{
    g_assert(i != NULL);
    if (G_UNLIKELY(i == NULL))
        return;
    if (*i < min)
        *i = min;
    if (*i > max)
        *i = max;
}


static void
xfceweather_read_config(XfcePanelPlugin *plugin,
                        plugin_data *data)
{
    XfceRc *rc;
    const gchar *value;
    gchar *file;
    gchar label[10];
    gint label_count = 0, val;

    if (!(file = xfce_panel_plugin_lookup_rc_file(plugin)))
        return;

    rc = xfce_rc_simple_open(file, TRUE);
    g_free(file);

    if (!rc)
        return;

    value = xfce_rc_read_entry(rc, "loc_name", NULL);
    if (value) {
        g_free(data->location_name);
        data->location_name = g_strdup(value);
    }

    value = xfce_rc_read_entry(rc, "lat", NULL);
    if (value) {
        g_free(data->lat);
        data->lat = g_strdup(value);
    }

    value = xfce_rc_read_entry(rc, "lon", NULL);
    if (value) {
        g_free(data->lon);
        data->lon = g_strdup(value);
    }

    data->msl = xfce_rc_read_int_entry(rc, "msl", 0);
    constrain_to_limits(&data->msl, -420, 10000);

    value = xfce_rc_read_entry(rc, "timezone", NULL);
    if (value) {
        g_free(data->timezone);
        data->timezone = g_strdup(value);
    }

    value = xfce_rc_read_entry(rc, "geonames_username", NULL);
    if (value) {
        g_free(data->geonames_username);
        data->geonames_username = g_strdup(value);
    }

    data->cache_file_max_age =
        xfce_rc_read_int_entry(rc, "cache_file_max_age", CACHE_FILE_MAX_AGE);

    data->power_saving = xfce_rc_read_bool_entry(rc, "power_saving", TRUE);

    if (data->units)
        g_slice_free(units_config, data->units);
    data->units = g_slice_new0(units_config);
    data->units->temperature =
        xfce_rc_read_int_entry(rc, "units_temperature", CELSIUS);
    data->units->pressure =
        xfce_rc_read_int_entry(rc, "units_pressure", HECTOPASCAL);
    data->units->windspeed =
        xfce_rc_read_int_entry(rc, "units_windspeed", KMH);
    data->units->precipitation =
        xfce_rc_read_int_entry(rc, "units_precipitation", MILLIMETERS);
    data->units->altitude =
        xfce_rc_read_int_entry(rc, "units_altitude", METERS);
    data->units->apparent_temperature =
        xfce_rc_read_int_entry(rc, "model_apparent_temperature",
                               STEADMAN);

    data->round = xfce_rc_read_bool_entry(rc, "round", TRUE);

    data->single_row = xfce_rc_read_bool_entry(rc, "single_row", TRUE);

    data->tooltip_style = xfce_rc_read_int_entry(rc, "tooltip_style",
                                                 TOOLTIP_VERBOSE);

    val = xfce_rc_read_int_entry(rc, "forecast_layout", FC_LAYOUT_LIST);
    if (val == FC_LAYOUT_CALENDAR || val == FC_LAYOUT_LIST)
        data->forecast_layout = val;
    else
        data->forecast_layout = FC_LAYOUT_LIST;

    data->forecast_days = xfce_rc_read_int_entry(rc, "forecast_days",
                                                 DEFAULT_FORECAST_DAYS);
    constrain_to_limits(&data->forecast_days, 1, MAX_FORECAST_DAYS);

    value = xfce_rc_read_entry(rc, "theme_dir", NULL);
    if (data->icon_theme)
        icon_theme_free(data->icon_theme);
    data->icon_theme = icon_theme_load(value);

    data->show_scrollbox = xfce_rc_read_bool_entry(rc, "show_scrollbox", TRUE);

    data->scrollbox_lines = xfce_rc_read_int_entry(rc, "scrollbox_lines", 1);
    constrain_to_limits(&data->scrollbox_lines, 1, MAX_SCROLLBOX_LINES);

    value = xfce_rc_read_entry(rc, "scrollbox_font", NULL);
    if (value) {
        g_free(data->scrollbox_font);
        data->scrollbox_font = g_strdup(value);
    }

    value = xfce_rc_read_entry(rc, "scrollbox_color", NULL);
    if (value)
        gdk_color_parse(value, &(data->scrollbox_color));

    data->scrollbox_use_color =
        xfce_rc_read_bool_entry(rc, "scrollbox_use_color", FALSE);

    data->scrollbox_animate =
        xfce_rc_read_bool_entry(rc, "scrollbox_animate", TRUE);
    gtk_scrollbox_set_animate(GTK_SCROLLBOX(data->scrollbox),
                              data->scrollbox_animate);

    data->labels = labels_clear(data->labels);
    val = 0;
    while (val != -1) {
        g_snprintf(label, 10, "label%d", label_count++);

        val = xfce_rc_read_int_entry(rc, label, -1);
        if (val >= 0)
            g_array_append_val(data->labels, val);
    }

    xfce_rc_close(rc);
    weather_debug("Config file read.");
}


static void
xfceweather_write_config(XfcePanelPlugin *plugin,
                         plugin_data *data)
{
    XfceRc *rc;
    gchar label[10];
    gchar *file, *value;
    gint i;

    if (!(file = xfce_panel_plugin_save_location(plugin, TRUE)))
        return;

    /* get rid of old values */
    unlink(file);

    rc = xfce_rc_simple_open(file, FALSE);
    g_free(file);

    if (!rc)
        return;

    if (data->location_name)
        xfce_rc_write_entry(rc, "loc_name", data->location_name);

    if (data->lat)
        xfce_rc_write_entry(rc, "lat", data->lat);

    if (data->lon)
        xfce_rc_write_entry(rc, "lon", data->lon);

    xfce_rc_write_int_entry(rc, "msl", data->msl);

    xfce_rc_write_entry(rc, "timezone", data->timezone);

    if (data->geonames_username)
        xfce_rc_write_entry(rc, "geonames_username", data->geonames_username);

    xfce_rc_write_int_entry(rc, "cache_file_max_age",
                            data->cache_file_max_age);

    xfce_rc_write_bool_entry(rc, "power_saving", data->power_saving);

    xfce_rc_write_int_entry(rc, "units_temperature", data->units->temperature);
    xfce_rc_write_int_entry(rc, "units_pressure", data->units->pressure);
    xfce_rc_write_int_entry(rc, "units_windspeed", data->units->windspeed);
    xfce_rc_write_int_entry(rc, "units_precipitation",
                            data->units->precipitation);
    xfce_rc_write_int_entry(rc, "units_altitude", data->units->altitude);
    xfce_rc_write_int_entry(rc, "model_apparent_temperature",
                            data->units->apparent_temperature);

    xfce_rc_write_bool_entry(rc, "round", data->round);

    xfce_rc_write_bool_entry(rc, "single_row", data->single_row);

    xfce_rc_write_int_entry(rc, "tooltip_style", data->tooltip_style);

    xfce_rc_write_int_entry(rc, "forecast_layout", data->forecast_layout);

    xfce_rc_write_int_entry(rc, "forecast_days", data->forecast_days);

    xfce_rc_write_bool_entry(rc, "scrollbox_animate", data->scrollbox_animate);

    if (data->icon_theme && data->icon_theme->dir)
        xfce_rc_write_entry(rc, "theme_dir", data->icon_theme->dir);

    xfce_rc_write_bool_entry(rc, "show_scrollbox",
                             data->show_scrollbox);

    xfce_rc_write_int_entry(rc, "scrollbox_lines", data->scrollbox_lines);

    if (data->scrollbox_font)
        xfce_rc_write_entry(rc, "scrollbox_font", data->scrollbox_font);

    value = gdk_color_to_string(&(data->scrollbox_color));
    xfce_rc_write_entry(rc, "scrollbox_color", value);
    g_free(value);

    xfce_rc_write_bool_entry(rc, "scrollbox_use_color",
                             data->scrollbox_use_color);

    for (i = 0; i < data->labels->len; i++) {
        g_snprintf(label, 10, "label%d", i);
        xfce_rc_write_int_entry(rc, label,
                                (gint) g_array_index(data->labels,
                                                     data_types, i));
    }

    xfce_rc_close(rc);
    weather_debug("Config file written.");
}


/*
 * Generate file name for the weather data cache file.
 */
static gchar *
make_cache_filename(plugin_data *data)
{
    gchar *cache_dir, *file;

    if (G_UNLIKELY(data->lat == NULL || data->lon == NULL))
        return NULL;

    cache_dir = get_cache_directory();
    file = g_strdup_printf("%s%sweatherdata_%s_%s_%d",
                           cache_dir, G_DIR_SEPARATOR_S,
                           data->lat, data->lon, data->msl);
    g_free(cache_dir);
    return file;
}


static void
write_cache_file(plugin_data *data)
{
    GString *out;
    xml_weather *wd = data->weatherdata;
    xml_time *timeslice;
    xml_location *loc;
    xml_astro *astro;
    gchar *file, *start, *end, *point, *now, *value;
    gchar *date_format = "%Y-%m-%dT%H:%M:%SZ";
    time_t now_t = time(NULL);
    gint i, j;

    file = make_cache_filename(data);
    if (G_UNLIKELY(file == NULL))
        return;

    out = g_string_sized_new(20480);
    g_string_assign(out, "# xfce4-weather-plugin cache file\n\n[info]\n");
    CACHE_APPEND("location_name=%s\n", data->location_name);
    CACHE_APPEND("lat=%s\n", data->lat);
    CACHE_APPEND("lon=%s\n", data->lon);
    g_string_append_printf(out, "msl=%d\n", data->msl);
    g_string_append_printf(out, "timeslices=%d\n", wd->timeslices->len);
    if (G_LIKELY(data->weather_update)) {
        value = format_date(data->weather_update->last, date_format, FALSE);
        CACHE_APPEND("last_weather_download=%s\n", value);
        g_free(value);
    }
    if (G_LIKELY(data->astro_update)) {
        value = format_date(data->astro_update->last, date_format, FALSE);
        CACHE_APPEND("last_astro_download=%s\n", value);
        g_free(value);
    }
    now = format_date(now_t, date_format, FALSE);
    CACHE_APPEND("cache_date=%s\n\n", now);
    g_free(now);

    if (data->astrodata) {
        for (i = 0; i < data->astrodata->len; i++) {
            astro = g_array_index(data->astrodata, xml_astro *, i);
            if (G_UNLIKELY(astro == NULL))
                continue;
            value = format_date(astro->day, "%Y-%m-%d", TRUE);
            start = format_date(astro->sunrise, date_format, FALSE);
            end = format_date(astro->sunset, date_format, FALSE);
            g_string_append_printf(out, "[astrodata%d]\n", i);
            CACHE_APPEND("day=%s\n", value);
            CACHE_APPEND("sunrise=%s\n", start);
            CACHE_APPEND("sunset=%s\n", end);
            CACHE_APPEND("sun_never_rises=%s\n",
                         astro->sun_never_rises ? "true" : "false");
            CACHE_APPEND("sun_never_sets=%s\n",
                         astro->sun_never_sets ? "true" : "false");
            g_free(value);
            g_free(start);
            g_free(end);

            start = format_date(astro->moonrise, date_format, FALSE);
            end = format_date(astro->moonset, date_format, FALSE);
            CACHE_APPEND("moonrise=%s\n", start);
            CACHE_APPEND("moonset=%s\n", end);
            CACHE_APPEND("moon_never_rises=%s\n",
                         astro->moon_never_rises ? "true" : "false");
            CACHE_APPEND("moon_never_sets=%s\n",
                         astro->moon_never_sets ? "true" : "false");
            CACHE_APPEND("moon_phase=%s\n", astro->moon_phase);
            g_free(start);
            g_free(end);

            g_string_append(out, "\n");
        }
    } else
        g_string_append(out, "\n");

    for (i = 0; i < wd->timeslices->len; i++) {
        timeslice = g_array_index(wd->timeslices, xml_time *, i);
        if (G_UNLIKELY(timeslice == NULL || timeslice->location == NULL))
            continue;
        loc = timeslice->location;
        start = format_date(timeslice->start, date_format, FALSE);
        end = format_date(timeslice->end, date_format, FALSE);
        point = format_date(timeslice->point, date_format, FALSE);
        g_string_append_printf(out, "[timeslice%d]\n", i);
        CACHE_APPEND("start=%s\n", start);
        CACHE_APPEND("end=%s\n", end);
        CACHE_APPEND("point=%s\n", point);
        CACHE_APPEND("altitude=%s\n", loc->altitude);
        CACHE_APPEND("latitude=%s\n", loc->latitude);
        CACHE_APPEND("longitude=%s\n", loc->longitude);
        CACHE_APPEND("temperature_value=%s\n", loc->temperature_value);
        CACHE_APPEND("temperature_unit=%s\n", loc->temperature_unit);
        CACHE_APPEND("wind_dir_deg=%s\n", loc->wind_dir_deg);
        CACHE_APPEND("wind_dir_name=%s\n", loc->wind_dir_name);
        CACHE_APPEND("wind_speed_mps=%s\n", loc->wind_speed_mps);
        CACHE_APPEND("wind_speed_beaufort=%s\n", loc->wind_speed_beaufort);
        CACHE_APPEND("humidity_value=%s\n", loc->humidity_value);
        CACHE_APPEND("humidity_unit=%s\n", loc->humidity_unit);
        CACHE_APPEND("pressure_value=%s\n", loc->pressure_value);
        CACHE_APPEND("pressure_unit=%s\n", loc->pressure_unit);
        g_free(start);
        g_free(end);
        g_free(point);
        for (j = 0; j < CLOUDS_PERC_NUM; j++)
            if (loc->clouds_percent[j])
                g_string_append_printf(out, "clouds_percent_%d=%s\n", j,
                                       loc->clouds_percent[j]);
        CACHE_APPEND("fog_percent=%s\n", loc->fog_percent);
        CACHE_APPEND("precipitation_value=%s\n", loc->precipitation_value);
        CACHE_APPEND("precipitation_unit=%s\n", loc->precipitation_unit);
        if (loc->symbol)
            g_string_append_printf(out, "symbol_id=%d\nsymbol=%s\n",
                                   loc->symbol_id, loc->symbol);
        g_string_append(out, "\n");
    }

    if (!g_file_set_contents(file, out->str, -1, NULL))
        g_warning(_("Error writing cache file %s!"), file);
    else
        weather_debug("Cache file %s has been written.", file);

    g_string_free(out, TRUE);
    g_free(file);
}


static void
read_cache_file(plugin_data *data)
{
    GKeyFile *keyfile;
    GError **err;
    xml_weather *wd;
    xml_time *timeslice = NULL;
    xml_location *loc = NULL;
    xml_astro *astro = NULL;
    time_t now_t = time(NULL), cache_date_t;
    gchar *file, *locname = NULL, *lat = NULL, *lon = NULL, *group = NULL;
    gchar *timestring;
    gint msl, num_timeslices = 0, i, j;

    g_assert(data != NULL);
    if (G_UNLIKELY(data == NULL))
        return;
    wd = data->weatherdata;

    if (G_UNLIKELY(data->lat == NULL || data->lon == NULL))
        return;

    file = make_cache_filename(data);
    if (G_UNLIKELY(file == NULL))
        return;

    keyfile = g_key_file_new();
    if (!g_key_file_load_from_file(keyfile, file, G_KEY_FILE_NONE, NULL)) {
        weather_debug("Could not read cache file %s.", file);
        g_free(file);
        return;
    }
    weather_debug("Reading cache file %s.", file);
    g_free(file);

    group = "info";
    if (!g_key_file_has_group(keyfile, group)) {
        CACHE_FREE_VARS();
        return;
    }

    /* check all needed values are present and match the current parameters */
    locname = g_key_file_get_string(keyfile, group, "location_name", NULL);
    lat = g_key_file_get_string(keyfile, group, "lat", NULL);
    lon = g_key_file_get_string(keyfile, group, "lon", NULL);
    if (locname == NULL || lat == NULL || lon == NULL) {
        CACHE_FREE_VARS();
        weather_debug("Required values are missing in the cache file, "
                      "reading cache file aborted.");
        return;
    }
    msl = g_key_file_get_integer(keyfile, group, "msl", err);
    if (!err)
        num_timeslices = g_key_file_get_integer(keyfile, group,
                                                "timeslices", err);
    if (err || strcmp(lat, data->lat) || strcmp(lon, data->lon) ||
        msl != data->msl || num_timeslices < 1) {
        CACHE_FREE_VARS();
        weather_debug("The required values are not present in the cache file "
                      "or do not match the current plugin data. Reading "
                      "cache file aborted.");
        return;
    }
    /* read cache creation date and check if cache file is not too old */
    CACHE_READ_STRING(timestring, "cache_date");
    cache_date_t = parse_timestring(timestring, NULL, FALSE);
    g_free(timestring);
    if (difftime(now_t, cache_date_t) > data->cache_file_max_age) {
        weather_debug("Cache file is too old and will not be used.");
        CACHE_FREE_VARS();
        return;
    }
    if (G_LIKELY(data->weather_update)) {
        CACHE_READ_STRING(timestring, "last_weather_download");
        data->weather_update->last = parse_timestring(timestring, NULL, FALSE);
        data->weather_update->next =
            calc_next_download_time(data->weather_update,
                                    data->weather_update->last);
        g_free(timestring);
    }
    if (G_LIKELY(data->astro_update)) {
        CACHE_READ_STRING(timestring, "last_astro_download");
        data->astro_update->last = parse_timestring(timestring, NULL, FALSE);
        data->astro_update->next =
            calc_next_download_time(data->astro_update,
                                    data->astro_update->last);
        g_free(timestring);
    }

    /* read cached astrodata if available and up-to-date */
    i = 0;
    group = g_strdup_printf("astrodata%d", i);
    while (g_key_file_has_group(keyfile, group)) {
        if (i == 0)
            weather_debug("Reusing cached astrodata instead of downloading it.");

        astro = g_slice_new0(xml_astro);
        if (G_UNLIKELY(astro == NULL))
            break;

        CACHE_READ_STRING(timestring, "day");
        astro->day = parse_timestring(timestring, "%Y-%m-%d", TRUE);
        g_free(timestring);
        CACHE_READ_STRING(timestring, "sunrise");
        astro->sunrise = parse_timestring(timestring, NULL, FALSE);
        g_free(timestring);
        CACHE_READ_STRING(timestring, "sunset");
        astro->sunset = parse_timestring(timestring, NULL, FALSE);
        g_free(timestring);
        astro->sun_never_rises =
            g_key_file_get_boolean(keyfile, group, "sun_never_rises", NULL);
        astro->sun_never_sets =
            g_key_file_get_boolean(keyfile, group, "sun_never_sets", NULL);

        CACHE_READ_STRING(timestring, "moonrise");
        astro->moonrise = parse_timestring(timestring, NULL, FALSE);
        g_free(timestring);
        CACHE_READ_STRING(timestring, "moonset");
        astro->moonset = parse_timestring(timestring, NULL, FALSE);
        g_free(timestring);
        CACHE_READ_STRING(astro->moon_phase, "moon_phase");
        astro->moon_never_rises =
            g_key_file_get_boolean(keyfile, group, "moon_never_rises", NULL);
        astro->moon_never_sets =
            g_key_file_get_boolean(keyfile, group, "moon_never_sets", NULL);

        merge_astro(data->astrodata, astro);
        xml_astro_free(astro);

        g_free(group);
        group = g_strdup_printf("astrodata%d", ++i);
    }
    g_free(group);
    group = NULL;

    /* parse available timeslices */
    for (i = 0; i < num_timeslices; i++) {
        group = g_strdup_printf("timeslice%d", i);
        if (!g_key_file_has_group(keyfile, group)) {
            weather_debug("Group %s not found, continuing with next.", group);
            g_free(group);
            continue;
        }

        timeslice = make_timeslice();
        if (G_UNLIKELY(timeslice == NULL)) {
            g_free(group);
            continue;
        }

        /* parse time strings (start, end, point) */
        CACHE_READ_STRING(timestring, "start");
        timeslice->start = parse_timestring(timestring, NULL, FALSE);
        g_free(timestring);
        CACHE_READ_STRING(timestring, "end");
        timeslice->end = parse_timestring(timestring, NULL, FALSE);
        g_free(timestring);
        CACHE_READ_STRING(timestring, "point");
        timeslice->point = parse_timestring(timestring, NULL, FALSE);
        g_free(timestring);

        /* parse location data */
        loc = timeslice->location;
        CACHE_READ_STRING(loc->altitude, "altitude");
        CACHE_READ_STRING(loc->latitude, "latitude");
        CACHE_READ_STRING(loc->longitude, "longitude");
        CACHE_READ_STRING(loc->temperature_value, "temperature_value");
        CACHE_READ_STRING(loc->temperature_unit, "temperature_unit");
        CACHE_READ_STRING(loc->wind_dir_name, "wind_dir_name");
        CACHE_READ_STRING(loc->wind_dir_deg, "wind_dir_deg");
        CACHE_READ_STRING(loc->wind_speed_mps, "wind_speed_mps");
        CACHE_READ_STRING(loc->wind_speed_beaufort, "wind_speed_beaufort");
        CACHE_READ_STRING(loc->humidity_value, "humidity_value");
        CACHE_READ_STRING(loc->humidity_unit, "humidity_unit");
        CACHE_READ_STRING(loc->pressure_value, "pressure_value");
        CACHE_READ_STRING(loc->pressure_unit, "pressure_unit");

        for (j = 0; j < CLOUDS_PERC_NUM; j++) {
            gchar *key = g_strdup_printf("clouds_percent_%d", j);
            if (g_key_file_has_key(keyfile, group, key, NULL))
                loc->clouds_percent[j] =
                    g_key_file_get_string(keyfile, group, key, NULL);
            g_free(key);
        }

        CACHE_READ_STRING(loc->fog_percent, "fog_percent");
        CACHE_READ_STRING(loc->precipitation_value, "precipitation_value");
        CACHE_READ_STRING(loc->precipitation_unit, "precipitation_unit");
        CACHE_READ_STRING(loc->symbol, "symbol");
        if (loc->symbol &&
            g_key_file_has_key(keyfile, group, "symbol_id", NULL))
            loc->symbol_id =
                g_key_file_get_integer(keyfile, group, "symbol_id", NULL);

        merge_timeslice(wd, timeslice);
        xml_time_free(timeslice);
    }
    CACHE_FREE_VARS();
    weather_debug("Reading cache file complete.");
}


void
update_weatherdata_with_reset(plugin_data *data)
{
    time_t now_t;
    GSource *source;

    weather_debug("Update weatherdata with reset.");
    g_assert(data != NULL);
    if (G_UNLIKELY(data == NULL))
        return;

    if (data->update_timer) {
        source = g_main_context_find_source_by_id(NULL, data->update_timer);
        if (source) {
            g_source_destroy(source);
            data->update_timer = 0;
        }
    }

    /* set location timezone */
    update_timezone(data);

    /* clear update times */
    init_update_infos(data);

    /* clear existing weather data */
    if (data->weatherdata) {
        xml_weather_free(data->weatherdata);
        data->weatherdata = make_weather_data();
    }

    /* clear existing astronomical data */
    if (data->astrodata) {
        astrodata_free(data->astrodata);
        data->astrodata = g_array_sized_new(FALSE, TRUE, sizeof(xml_astro *), 30);
    }

    /* update GUI to display NODATA */
    update_icon(data);
    update_scrollbox(data, TRUE);

    /* make use of previously saved data */
    read_cache_file(data);

    /* schedule downloads immediately */
    time(&now_t);
    data->weather_update->next = now_t;
    data->astro_update->next = now_t;
    schedule_next_wakeup(data);

    weather_debug("Updated weatherdata with reset.");
}


/* This is only a dummy handler, the clicks will be processed by
   cb_click. This is needed to synchronise the toggled state with
   the existence of the summary window. */
static gboolean
cb_toggled(GtkToggleButton *button,
           gpointer user_data)
{
    plugin_data *data = (plugin_data *) user_data;
    g_signal_handlers_block_by_func(data->button, cb_toggled, data);
    if (data->summary_window)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->button), TRUE);
    else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->button), FALSE);
    g_signal_handlers_unblock_by_func(data->button, cb_toggled, data);
    return FALSE;
}


static void
close_summary(GtkWidget *widget,
              gpointer *user_data)
{
    plugin_data *data = (plugin_data *) user_data;
    GSource *source;

    if (data->summary_details)
        summary_details_free(data->summary_details);
    data->summary_details = NULL;
    data->summary_window = NULL;

    /* deactivate the summary window update timer */
    if (data->summary_update_timer) {
        source = g_main_context_find_source_by_id(NULL,
                                                  data->summary_update_timer);
        if (source) {
            g_source_destroy(source);
            data->summary_update_timer = 0;
        }
    }

    /* sync toggle button state */
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->button), FALSE);
}


void
forecast_click(GtkWidget *widget,
               gpointer user_data)
{
    plugin_data *data = (plugin_data *) user_data;

    if (data->summary_window != NULL)
        gtk_widget_destroy(data->summary_window);
    else {
        /* sync toggle button state */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(data->button), TRUE);

        data->summary_window = create_summary_window(data);

        /* start the summary window subtitle update timer */
        update_summary_subtitle(data);

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
    plugin_data *data = (plugin_data *) user_data;

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
    plugin_data *data = (plugin_data *) user_data;

    if (event->direction == GDK_SCROLL_UP)
        gtk_scrollbox_next_label(GTK_SCROLLBOX(data->scrollbox));
    else if (event->direction == GDK_SCROLL_DOWN)
        gtk_scrollbox_prev_label(GTK_SCROLLBOX(data->scrollbox));

    return FALSE;
}


static void
mi_click(GtkWidget *widget,
         gpointer user_data)
{
    plugin_data *data = (plugin_data *) user_data;

    update_weatherdata_with_reset(data);
}

static void
proxy_auth(SoupSession *session,
           SoupMessage *msg,
           SoupAuth *auth,
           gboolean retrying,
           gpointer user_data)
{
    SoupURI *soup_proxy_uri;
    const gchar *proxy_uri;

    if (!retrying) {
        if (msg->status_code == SOUP_STATUS_PROXY_AUTHENTICATION_REQUIRED) {
            proxy_uri = g_getenv("HTTP_PROXY");
            if (!proxy_uri)
                proxy_uri = g_getenv("http_proxy");
            if (proxy_uri) {
                soup_proxy_uri = soup_uri_new(proxy_uri);
                soup_auth_authenticate(auth,
                                       soup_uri_get_user(soup_proxy_uri),
                                       soup_uri_get_password(soup_proxy_uri));
                soup_uri_free(soup_proxy_uri);
            }
        }
    }
}


#ifdef HAVE_UPOWER_GLIB
static void
#if UP_CHECK_VERSION(0, 99, 0)
upower_changed_cb(UpClient *client,
                  GParamSpec *pspec,
                  plugin_data *data)
#else /* UP_CHECK_VERSION < 0.99 */
upower_changed_cb(UpClient *client,
                  plugin_data *data)
#endif /* UP_CHECK_VERSION */
{
    gboolean on_battery;

    if (G_UNLIKELY(data->upower == NULL) || !data->power_saving)
        return;

    on_battery = data->upower_on_battery;
    weather_debug("upower old status: on_battery=%d", on_battery);

    data->upower_on_battery = up_client_get_on_battery(client);
    weather_debug("upower new status: on_battery=%d", data->upower_on_battery);

    if (data->upower_on_battery != on_battery) {
        if (data->summary_window)
            update_summary_subtitle(data);

        update_icon(data);
        update_scrollbox(data, FALSE);
        schedule_next_wakeup(data);
    }
}
#endif /* HAVE_UPOWER_GLIB */


static void
xfceweather_dialog_response(GtkWidget *dlg,
                            gint response,
                            xfceweather_dialog *dialog)
{
    plugin_data *data = (plugin_data *) dialog->pd;
    icon_theme *theme;
    gboolean result;
    gint i;

    if (response == GTK_RESPONSE_HELP) {
        /* show help */
        result = g_spawn_command_line_async("exo-open --launch WebBrowser "
                                            PLUGIN_WEBSITE, NULL);

        if (G_UNLIKELY(result == FALSE))
            g_warning(_("Unable to open the following url: %s"),
                      PLUGIN_WEBSITE);
    } else {
        /* free stuff used in config dialog */
        gtk_widget_destroy(dlg);
        gtk_list_store_clear(dialog->model_datatypes);
        for (i = 0; i < dialog->icon_themes->len; i++) {
            theme = g_array_index(dialog->icon_themes, icon_theme *, i);
            icon_theme_free(theme);
            g_array_free(dialog->icon_themes, TRUE);
        }
        g_slice_free(xfceweather_dialog, dialog);

        xfce_panel_plugin_unblock_menu(data->plugin);

        weather_debug("Saving configuration options.");
        xfceweather_write_config(data->plugin, data);
        weather_dump(weather_dump_plugindata, data);
    }
}


static void
xfceweather_create_options(XfcePanelPlugin *plugin,
                           plugin_data *data)
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
    gtk_widget_show(dlg);
}


static gchar *
weather_get_tooltip_text(const plugin_data *data)
{
    xml_time *conditions;
    gchar *text, *sym, *alt, *temp;
    gchar *windspeed, *windbeau, *winddir, *winddeg;
    gchar *pressure, *humidity, *precipitation;
    gchar *fog, *cloudiness, *sunval, *value;
    gchar *point, *interval_start, *interval_end, *sunrise, *sunset;
    const gchar *unit;

    conditions = get_current_conditions(data->weatherdata);
    if (G_UNLIKELY(conditions == NULL)) {
        text = g_strdup(_("Short-term forecast data unavailable."));
        return text;
    }

    /* times for forecast and point data */
    point = format_date(conditions->point, "%H:%M", TRUE);
    interval_start = format_date(conditions->start, "%H:%M", TRUE);
    interval_end = format_date(conditions->end, "%H:%M", TRUE);

    /* use sunrise and sunset times if available */
    if (data->current_astro)
        if (data->current_astro->sun_never_rises) {
            sunval = g_strdup(_("The sun never rises today."));
        } else if (data->current_astro->sun_never_sets) {
            sunval = g_strdup(_("The sun never sets today."));
        } else {
            sunrise = format_date(data->current_astro->sunrise,
                                  "%H:%M:%S", TRUE);
            sunset = format_date(data->current_astro->sunset,
                                 "%H:%M:%S", TRUE);
            sunval =
                g_strdup_printf(_("The sun rises at %s and sets at %s."),
                                sunrise, sunset);
            g_free(sunrise);
            g_free(sunset);
        }
    else
        sunval = g_strdup_printf("");

    sym = get_data(conditions, data->units, SYMBOL, FALSE, data->night_time);
    DATA_AND_UNIT(alt, ALTITUDE);
    DATA_AND_UNIT(temp, TEMPERATURE);
    DATA_AND_UNIT(windspeed, WIND_SPEED);
    DATA_AND_UNIT(windbeau, WIND_BEAUFORT);
    DATA_AND_UNIT(winddir, WIND_DIRECTION);
    DATA_AND_UNIT(winddeg, WIND_DIRECTION_DEG);
    DATA_AND_UNIT(pressure, PRESSURE);
    DATA_AND_UNIT(humidity, HUMIDITY);
    DATA_AND_UNIT(precipitation, PRECIPITATION);
    DATA_AND_UNIT(fog, FOG);
    DATA_AND_UNIT(cloudiness, CLOUDINESS);

    switch (data->tooltip_style) {
    case TOOLTIP_SIMPLE:
        text = g_markup_printf_escaped
            /*
             * TRANSLATORS: This is the simple tooltip. For a bigger challenge,
             * look at the verbose tooltip style further below ;-)
             */
            (_("<b><span size=\"large\">%s</span></b> "
               "<span size=\"medium\">(%s)</span>\n"
               "<b><span size=\"large\">%s</span></b>\n\n"
               "<b>Temperature:</b> %s\n"
               "<b>Wind:</b> %s from %s\n"
               "<b>Pressure:</b> %s\n"
               "<b>Humidity:</b> %s\n"),
             data->location_name, alt,
             translate_desc(sym, data->night_time),
             temp, windspeed, winddir, pressure, humidity);
        break;

    case TOOLTIP_VERBOSE:
    default:
        text = g_markup_printf_escaped
            /*
             * TRANSLATORS: Re-arrange and align at will, optionally using
             * abbreviations for labels if desired or necessary. Just take
             * into account the possible size constraints, the centered
             * vertical alignment of the icon - which unfortunately cannot
             * be changed easily - and try to make it compact and look
             * good! The missing space after "%son the ..." is intentional,
             * it is included in the %s.
             */
            (_("<b><span size=\"large\">%s</span></b> "
               "<span size=\"medium\">(%s)</span>\n"
               "<b><span size=\"large\">%s</span></b>\n"
               "<span size=\"smaller\">"
               "from %s to %s, with %s of precipitation</span>\n\n"
               "<b>Temperature:</b> %s\t\t"
               "<span size=\"smaller\">(values at %s)</span>\n"
               "<b>Wind:</b> %s (%son the Beaufort scale) from %s(%s)\n"
               "<b>Pressure:</b> %s    <b>Humidity:</b> %s\n"
               "<b>Fog:</b> %s    <b>Cloudiness:</b> %s\n\n"
               "<span size=\"smaller\">%s</span>"),
             data->location_name, alt,
             translate_desc(sym, data->night_time),
             interval_start, interval_end,
             precipitation,
             temp, point,
             windspeed, windbeau, winddir, winddeg,
             pressure, humidity,
             fog, cloudiness,
             sunval);
        break;
    }
    g_free(sunval);
    g_free(sym);
    g_free(alt);
    g_free(temp);
    g_free(interval_start);
    g_free(interval_end);
    g_free(point);
    g_free(windspeed);
    g_free(windbeau);
    g_free(winddir);
    g_free(winddeg);
    g_free(pressure);
    g_free(humidity);
    g_free(precipitation);
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
                       plugin_data *data)
{
    gchar *markup_text;

    if (data->weatherdata == NULL)
        gtk_tooltip_set_text(tooltip, _("Cannot update weather data"));
    else {
        markup_text = weather_get_tooltip_text(data);
        gtk_tooltip_set_markup(tooltip, markup_text);
        g_free(markup_text);
    }

    gtk_tooltip_set_icon(tooltip, data->tooltip_icon);
    return TRUE;
}


static plugin_data *
xfceweather_create_control(XfcePanelPlugin *plugin)
{
    plugin_data *data = g_slice_new0(plugin_data);
    SoupURI *soup_proxy_uri;
    const gchar *proxy_uri;
    const gchar *proxy_user;
    GtkWidget *refresh, *refresh_icon;
    GdkPixbuf *icon = NULL;
    data_types lbl;

    /* Initialize with sane default values */
    data->plugin = plugin;
#ifdef HAVE_UPOWER_GLIB
    data->upower = up_client_new();
    if (data->upower)
        data->upower_on_battery = up_client_get_on_battery(data->upower);
#endif
    data->units = g_slice_new0(units_config);
    data->weatherdata = make_weather_data();
    data->astrodata = g_array_sized_new(FALSE, TRUE, sizeof(xml_astro *), 30);
    data->cache_file_max_age = CACHE_FILE_MAX_AGE;
    data->show_scrollbox = TRUE;
    data->scrollbox_lines = 1;
    data->scrollbox_animate = TRUE;
    data->tooltip_style = TOOLTIP_VERBOSE;
    data->forecast_layout = FC_LAYOUT_LIST;
    data->forecast_days = DEFAULT_FORECAST_DAYS;
    data->round = TRUE;
    data->single_row = TRUE;
    data->power_saving = TRUE;

    /* Setup update infos */
    init_update_infos(data);
    data->next_wakeup = time(NULL);

    /* Setup session for HTTP connections */
    data->session = soup_session_async_new();
    g_object_set(data->session, SOUP_SESSION_TIMEOUT,
                 CONN_TIMEOUT, NULL);

    /* Set the proxy URI from environment */
    proxy_uri = g_getenv("HTTP_PROXY");
    if (!proxy_uri)
        proxy_uri = g_getenv("http_proxy");
    if (proxy_uri) {
        soup_proxy_uri = soup_uri_new(proxy_uri);
        g_object_set(data->session, SOUP_SESSION_PROXY_URI,
                     soup_proxy_uri, NULL);

        /* check if uri contains authentication info */
        proxy_user = soup_uri_get_user(soup_proxy_uri);
        if (proxy_user && strlen(proxy_user) > 0) {
            g_signal_connect(G_OBJECT(data->session), "authenticate",
                             G_CALLBACK(proxy_auth), NULL);
        }

        soup_uri_free(soup_proxy_uri);
    }

    data->scrollbox = gtk_scrollbox_new();

    data->panel_size = xfce_panel_plugin_get_size(plugin);
    data->panel_rows = xfce_panel_plugin_get_nrows(plugin);
    data->icon_theme = icon_theme_load(NULL);
    icon = get_icon(data->icon_theme, NULL, 16, FALSE);
    if (G_LIKELY(icon)) {
        data->iconimage = gtk_image_new_from_pixbuf(icon);
        g_object_unref(G_OBJECT(icon));
    } else
        g_warning(_("No default icon theme? "
                    "This should not happen, plugin will crash!"));

    data->labels = g_array_new(FALSE, TRUE, sizeof(data_types));

    /* create panel toggle button which will contain all other widgets */
    data->button = xfce_create_panel_toggle_button();
    gtk_container_add(GTK_CONTAINER(plugin), data->button);
    xfce_panel_plugin_add_action_widget(plugin, data->button);
    gtk_widget_show(data->button);

    /* create alignment box that can be easily adapted to the panel
       orientation */
    data->alignbox = xfce_hvbox_new(GTK_ORIENTATION_HORIZONTAL, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(data->button), data->alignbox);

    /* add widgets to alignment box */
    data->vbox_center_scrollbox = gtk_vbox_new(FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(data->iconimage), 1, 0.5);
    gtk_box_pack_start(GTK_BOX(data->alignbox),
                       data->iconimage, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(data->vbox_center_scrollbox),
                       data->scrollbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(data->alignbox),
                       data->vbox_center_scrollbox, TRUE, TRUE, 0);
    gtk_widget_show_all(data->alignbox);

    /* hook up events for the button */
    g_object_set(G_OBJECT(data->button), "has-tooltip", TRUE, NULL);
    g_signal_connect(G_OBJECT(data->button), "query-tooltip",
                     G_CALLBACK(weather_get_tooltip_cb), data);
    g_signal_connect(G_OBJECT(data->button), "button-press-event",
                     G_CALLBACK(cb_click), data);
    g_signal_connect(G_OBJECT(data->button), "scroll-event",
                     G_CALLBACK(cb_scroll), data);
    g_signal_connect(G_OBJECT(data->button), "toggled",
                     G_CALLBACK(cb_toggled), data);
    gtk_widget_add_events(data->scrollbox, GDK_BUTTON_PRESS_MASK);

    /* add refresh button to right click menu, for people who missed
       the middle mouse click feature */
    refresh = gtk_image_menu_item_new_with_mnemonic(_("Re_fresh"));
    refresh_icon = gtk_image_new_from_stock(GTK_STOCK_REFRESH,
                                            GTK_ICON_SIZE_MENU);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(refresh), refresh_icon);
    gtk_widget_show(refresh);
    g_signal_connect(G_OBJECT(refresh), "activate",
                     G_CALLBACK(mi_click), data);
    xfce_panel_plugin_menu_insert_item(plugin, GTK_MENU_ITEM(refresh));

    /* assign to tempval because g_array_append_val() is using & operator */
    lbl = TEMPERATURE;
    g_array_append_val(data->labels, lbl);
    lbl = WIND_DIRECTION;
    g_array_append_val(data->labels, lbl);
    lbl = WIND_SPEED;
    g_array_append_val(data->labels, lbl);

    weather_debug("Plugin widgets set up and ready.");
    return data;
}


static void
xfceweather_free(XfcePanelPlugin *plugin,
                 plugin_data *data)
{
    GSource *source;

    weather_debug("Freeing plugin data.");
    g_assert(data != NULL);

    if (data->update_timer) {
        source = g_main_context_find_source_by_id(NULL, data->update_timer);
        if (source) {
            g_source_destroy(source);
            data->update_timer = 0;
        }
    }

#ifdef HAVE_UPOWER_GLIB
    if (data->upower)
        g_object_unref(data->upower);
#endif

    if (data->weatherdata)
        xml_weather_free(data->weatherdata);

    if (data->units)
        g_slice_free(units_config, data->units);

    xmlCleanupParser();

    /* free chars */
    g_free(data->lat);
    g_free(data->lon);
    g_free(data->location_name);
    g_free(data->scrollbox_font);
    g_free(data->timezone);
    g_free(data->timezone_initial);
    g_free(data->geonames_username);

    /* free update infos */
    g_slice_free(update_info, data->weather_update);
    g_slice_free(update_info, data->astro_update);
    g_slice_free(update_info, data->conditions_update);

    /* free current data */
    data->current_astro = NULL;

    /* free arrays */
    g_array_free(data->labels, TRUE);
    astrodata_free(data->astrodata);

    /* free icon theme */
    icon_theme_free(data->icon_theme);

    g_slice_free(plugin_data, data);
}


static gboolean
xfceweather_set_size(XfcePanelPlugin *panel,
                     gint size,
                     plugin_data *data)
{
#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    data->panel_rows = xfce_panel_plugin_get_nrows(panel);
    if (data->single_row)
        size /= data->panel_rows;
#endif
    data->panel_size = size;

    update_icon(data);
    update_scrollbox(data, FALSE);

    weather_dump(weather_dump_plugindata, data);

    /* we handled the size */
    return TRUE;
}


#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
gboolean
xfceweather_set_mode(XfcePanelPlugin *panel,
                     XfcePanelPluginMode mode,
                     plugin_data *data)
{
    data->panel_orientation = xfce_panel_plugin_get_mode(panel);

    if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL ||
        (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_DESKBAR &&
         data->single_row)) {
        xfce_hvbox_set_orientation(XFCE_HVBOX(data->alignbox),
                                   GTK_ORIENTATION_HORIZONTAL);
        gtk_misc_set_alignment(GTK_MISC(data->iconimage), 1, 0.5);
    } else {
        xfce_hvbox_set_orientation(XFCE_HVBOX(data->alignbox),
                                   GTK_ORIENTATION_VERTICAL);
        gtk_misc_set_alignment(GTK_MISC(data->iconimage), 0.5, 1);
    }

    if (mode == XFCE_PANEL_PLUGIN_MODE_DESKBAR)
        xfce_panel_plugin_set_small(panel, FALSE);
    else
        xfce_panel_plugin_set_small(panel, data->single_row);

    gtk_scrollbox_set_orientation(GTK_SCROLLBOX(data->scrollbox),
                                  (mode != XFCE_PANEL_PLUGIN_MODE_VERTICAL)
                                  ? GTK_ORIENTATION_HORIZONTAL
                                  : GTK_ORIENTATION_VERTICAL);

    xfceweather_set_size(panel, xfce_panel_plugin_get_size(panel), data);

    weather_dump(weather_dump_plugindata, data);

    /* we handled the orientation */
    return TRUE;
}


#else


static gboolean
xfceweather_set_orientation(XfcePanelPlugin *panel,
                            GtkOrientation orientation,
                            plugin_data *data)
{
    data->panel_orientation = orientation;

    if (data->panel_orientation == GTK_ORIENTATION_HORIZONTAL) {
        xfce_hvbox_set_orientation(XFCE_HVBOX(data->alignbox),
                                   GTK_ORIENTATION_HORIZONTAL);
        gtk_misc_set_alignment(GTK_MISC(data->iconimage), 1, 0.5);
    } else {
        xfce_hvbox_set_orientation(XFCE_HVBOX(data->alignbox),
                                   GTK_ORIENTATION_VERTICAL);
        gtk_misc_set_alignment(GTK_MISC(data->iconimage), 0.5, 1);
    }
    gtk_scrollbox_set_orientation(GTK_SCROLLBOX(data->scrollbox),
                                  data->panel_orientation);

    update_icon(data);
    update_scrollbox(data, FALSE);

    weather_dump(weather_dump_plugindata, data);

    /* we handled the orientation */
    return TRUE;
}
#endif


static void
xfceweather_show_about(XfcePanelPlugin *plugin,
                       plugin_data *data)
{
    GdkPixbuf *icon;
    const gchar *auth[] = {
        "Bob Schlärmann <weatherplugin@atreidis.nl.eu.org>",
        "Benedikt Meurer <benny@xfce.org>",
        "Jasper Huijsmans <jasper@xfce.org>",
        "Masse Nicolas <masse_nicolas@yahoo.fr>",
        "Nick Schermer <nick@xfce.org>",
        "Colin Leroy <colin@colino.net>",
        "Harald Judt <h.judt@gmx.at>",
        NULL };
    icon = xfce_panel_pixbuf_from_source("xfce4-weather", NULL, 48);
    gtk_show_about_dialog
        (NULL,
         "logo", icon,
         "license", xfce_get_license_text(XFCE_LICENSE_TEXT_GPL),
         "version", PACKAGE_VERSION,
         "program-name", PACKAGE_NAME,
         "comments", _("Show weather conditions and forecasts"),
         "website", PLUGIN_WEBSITE,
         "copyright", _("Copyright (c) 2003-2014\n"),
         "authors", auth,
         NULL);

    if (icon)
        g_object_unref(G_OBJECT(icon));
}


static void
weather_construct(XfcePanelPlugin *plugin)
{
    plugin_data *data;
    const gchar *panel_debug_env;

    /* Enable debug level logging if PANEL_DEBUG contains G_LOG_DOMAIN */
    panel_debug_env = g_getenv("PANEL_DEBUG");
    if (panel_debug_env && strstr(panel_debug_env, G_LOG_DOMAIN))
        debug_mode = TRUE;
    weather_debug_init(G_LOG_DOMAIN, debug_mode);
    weather_debug("weather plugin version " VERSION " starting up");

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
    data = xfceweather_create_control(plugin);

    /* save initial timezone so we can reset it later */
    data->timezone_initial = g_strdup(g_getenv("TZ"));

    xfceweather_read_config(plugin, data);
    update_timezone(data);
    read_cache_file(data);
    update_current_conditions(data, TRUE);
    scrollbox_set_visible(data);
    gtk_scrollbox_set_fontname(GTK_SCROLLBOX(data->scrollbox),
                               data->scrollbox_font);
    if (data->scrollbox_use_color)
        gtk_scrollbox_set_color(GTK_SCROLLBOX(data->scrollbox),
                                data->scrollbox_color);

#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    xfceweather_set_mode(plugin, xfce_panel_plugin_get_mode(plugin), data);
#else
    xfceweather_set_orientation(plugin, xfce_panel_plugin_get_orientation(plugin), data);
#endif
    xfceweather_set_size(plugin, data->panel_size, data);

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

    xfce_panel_plugin_menu_show_about(plugin);
    g_signal_connect(G_OBJECT(plugin), "about",
                     G_CALLBACK(xfceweather_show_about), data);

#ifdef HAVE_UPOWER_GLIB
    if (data->upower) {
#if UP_CHECK_VERSION(0, 99, 0)
        g_signal_connect (data->upower, "notify",
                          G_CALLBACK(upower_changed_cb), data);
#else /* UP_CHECK_VERSION < 0.99 */
        g_signal_connect (data->upower, "changed",
                          G_CALLBACK(upower_changed_cb), data);
#endif /* UP_CHECK_VERSION */
    }
#endif /* HAVE_UPOWER_GLIB */

    weather_dump(weather_dump_plugindata, data);
}

XFCE_PANEL_PLUGIN_REGISTER(weather_construct)
