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

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <glib.h>
#include <stdarg.h>

#include "weather-debug.h"

#define YESNO(bool) ((bool) ? "yes" : "no")


static void
weather_dummy_log_handler(const gchar *log_domain,
                          const GLogLevelFlags log_level,
                          const gchar *message,
                          const gpointer unused_data)
{
    /* Swallow all messages */
}


void
weather_debug_init(const gchar *log_domain,
                   const gboolean debug_mode)
{
    /*
     * GLIB >= 2.32 only shows debug messages if G_MESSAGES_DEBUG contains the
     * log domain or "all", earlier GLIB versions always show debugging output
     */
#if GLIB_CHECK_VERSION(2,32,0)
    const gchar *debug_env;
    gchar *debug_env_new_array[] = { NULL, NULL, NULL, NULL };
    gchar *debug_env_new;
    gint i = 0, j = 0;

    if (debug_mode) {
        debug_env = g_getenv("G_MESSAGES_DEBUG");

        if (log_domain == NULL)
            debug_env_new_array[i++] = g_strdup("all");
        else {
            if (debug_env != NULL)
                debug_env_new_array[i++] = g_strdup(debug_env);
            if (debug_env == NULL || strstr(debug_env, log_domain) == NULL)
                debug_env_new_array[i++] = g_strdup(log_domain);
            if (debug_env == NULL || strstr(debug_env, G_LOG_DOMAIN) == NULL)
                debug_env_new_array[i++] = g_strdup(G_LOG_DOMAIN);
        }
        debug_env_new = g_strjoinv(" ", debug_env_new_array);
        g_setenv("G_MESSAGES_DEBUG", debug_env_new, TRUE);
        g_free(debug_env_new);

        while (j < i)
            g_free(debug_env_new_array[j++]);
    }
#else
    if (!debug_mode) {
        g_log_set_handler(log_domain, G_LOG_LEVEL_DEBUG,
                          weather_dummy_log_handler, NULL);
        g_log_set_handler(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG,
                          weather_dummy_log_handler, NULL);
    }
#endif
}


void
weather_debug_real(const gchar *log_domain,
                   const gchar *file,
                   const gchar *func,
                   const gint line,
                   const gchar *format,
                   ...)
{
    va_list args;
    gchar *prefixed_format;

    va_start(args, format);
    prefixed_format = g_strdup_printf("[%s:%d %s]: %s",
                                      file, line, func, format);
    g_logv(log_domain, G_LOG_LEVEL_DEBUG, prefixed_format, args);
    va_end(args);
    g_free(prefixed_format);
}


gchar *
weather_dump_geolocation(const xml_geolocation *geo)
{
    gchar *out;

    if (!geo)
        return g_strdup("No geolocation data.");

    out = g_strdup_printf("Geolocation data:\n"
                          "  --------------------------------------------\n"
                          "  city: %s\n"
                          "  country name: %s\n"
                          "  country code: %s\n"
                          "  region name: %s\n"
                          "  latitude: %s\n"
                          "  longitude: %s\n"
                          "  --------------------------------------------",
                          geo->city,
                          geo->country_name,
                          geo->country_code,
                          geo->region_name,
                          geo->latitude,
                          geo->longitude);
    return out;
}


gchar *
weather_dump_place(const xml_place *place)
{
    gchar *out;

    if (!place)
        return g_strdup("No place data.");

    out = g_strdup_printf("Place data:\n"
                          "  --------------------------------------------\n"
                          "  display_name: %s\n"
                          "  latitude: %s\n"
                          "  longitude: %s\n"
                          "  --------------------------------------------",
                          place->display_name,
                          place->lat,
                          place->lon);
    return out;
}


gchar *
weather_dump_timezone(const xml_timezone *tz)
{
    gchar *out;

    if (!tz)
        return g_strdup("No timezone data.");

    out = g_strdup_printf("Timezone data:\n"
                          "  --------------------------------------------\n"
                          "  country_code: %s\n"
                          "  country_name: %s\n"
                          "  timezone_id: %s\n"
                          "  --------------------------------------------",
                          tz->country_code,
                          tz->country_name,
                          tz->timezone_id);
    return out;
}


gchar *
weather_dump_icon_theme(const icon_theme *theme)
{
    gchar *out;

    if (!theme)
        return g_strdup("No icon theme data.");

    out = g_strdup_printf("Icon theme data:\n"
                          "  --------------------------------------------\n"
                          "  Dir: %s\n"
                          "  Name: %s\n"
                          "  Author: %s\n"
                          "  Description: %s\n"
                          "  License: %s\n"
                          "  --------------------------------------------",
                          theme->dir,
                          theme->name,
                          theme->author,
                          theme->description,
                          theme->license);
    return out;
}


gchar *
weather_dump_astrodata(const GArray *astrodata)
{
    GString *out;
    gchar *result, *line;
    xml_astro *astro;
    gint i;

    if (!astrodata || astrodata->len <= 0)
        return g_strdup("No astronomical data available.");

    out = g_string_sized_new(1024);
    g_string_assign(out, "Astronomical data:\n");
    for (i = 0; i < astrodata->len; i++) {
        astro = g_array_index(astrodata, xml_astro *, i);
        line = weather_dump_astro(astro);
        g_string_append(out, line);
        g_free(line);
    }
    /* Free GString only and return its character data */
    result = out->str;
    g_string_free(out, FALSE);
    return result;
}


gchar *
weather_dump_astro(const xml_astro *astro)
{
    gchar *out, *day, *sunrise, *sunset, *moonrise, *moonset;

    if (!astro)
        return g_strdup("Astrodata: NULL.");

    day = format_date(astro->day, "%c", TRUE);
    sunrise = format_date(astro->sunrise, "%c", TRUE);
    sunset = format_date(astro->sunset, "%c", TRUE);
    moonrise = format_date(astro->moonrise, "%c", TRUE);
    moonset = format_date(astro->moonset, "%c", TRUE);

    out = g_strdup_printf("day=%s, sun={%s, %s, %s, %s}, "
                          "moon={%s, %s, %s, %s, %s}\n",
                          day,
                          sunrise,
                          sunset,
                          YESNO(astro->sun_never_rises),
                          YESNO(astro->sun_never_sets),
                          moonrise,
                          moonset,
                          YESNO(astro->moon_never_rises),
                          YESNO(astro->moon_never_sets),
                          astro->moon_phase);

    g_free(day);
    g_free(sunrise);
    g_free(sunset);
    g_free(moonrise);
    g_free(moonset);
    return out;
}


static gchar *
weather_dump_location(const xml_location *loc,
                      const gboolean interval)
{
    gchar *out;

    if (!loc)
        return g_strdup("No location data.");

    if (interval)
        out =
            g_strdup_printf("alt=%s, lat=%s, lon=%s, "
                            "prec=%s %s, symid=%d (%s)",
                            loc->altitude,
                            loc->latitude,
                            loc->longitude,
                            loc->precipitation_value,
                            loc->precipitation_unit,
                            loc->symbol_id,
                            loc->symbol);
    else
        out =
            g_strdup_printf("alt=%s, lat=%s, lon=%s, temp=%s %s, "
                            "wind=%s %s° %s m/s (%s bf), "
                            "hum=%s %s, press=%s %s, fog=%s, cloudiness=%s, "
                            "cl=%s, cm=%s, ch=%s)",
                            loc->altitude,
                            loc->latitude,
                            loc->longitude,
                            loc->temperature_value,
                            loc->temperature_unit,
                            loc->wind_dir_name,
                            loc->wind_dir_deg,
                            loc->wind_speed_mps,
                            loc->wind_speed_beaufort,
                            loc->humidity_value,
                            loc->humidity_unit,
                            loc->pressure_value,
                            loc->pressure_unit,
                            loc->fog_percent,
                            loc->clouds_percent[CLOUDS_PERC_CLOUDINESS],
                            loc->clouds_percent[CLOUDS_PERC_LOW],
                            loc->clouds_percent[CLOUDS_PERC_MID],
                            loc->clouds_percent[CLOUDS_PERC_HIGH]);
    return out;
}


gchar *
weather_dump_units_config(const units_config *units)
{
    gchar *out;

    if (!units)
        return g_strdup("No units configuration data.");

    out = g_strdup_printf("Units configuration data:\n"
                          "  --------------------------------------------\n"
                          "  Temperature: %d\n"
                          "  Atmospheric pressure: %d\n"
                          "  Windspeed: %d\n"
                          "  Precipitation: %d\n"
                          "  Altitude: %d\n"
                          "  --------------------------------------------",
                          units->temperature,
                          units->pressure,
                          units->windspeed,
                          units->precipitation,
                          units->altitude);
    return out;
}


gchar *
weather_dump_timeslice(const xml_time *timeslice)
{
    GString *out;
    gchar *start, *end, *loc, *result;
    gboolean is_interval;

    if (G_UNLIKELY(timeslice == NULL))
        return g_strdup("No timeslice data.");

    out = g_string_sized_new(512);
    start = format_date(timeslice->start, "%c", TRUE);
    end = format_date(timeslice->end, "%c", TRUE);
    is_interval = (gboolean) strcmp(start, end);
    loc = weather_dump_location((timeslice) ? timeslice->location : NULL,
                                is_interval);
    g_string_append_printf(out, "[%s %s %s] %s\n", start,
                           is_interval ? "-" : "=", end, loc);
    g_free(start);
    g_free(end);
    g_free(loc);

    /* Free GString only and return its character data */
    result = out->str;
    g_string_free(out, FALSE);
    return result;
}


gchar *
weather_dump_weatherdata(const xml_weather *wd)
{
    GString *out;
    xml_time *timeslice;
    gchar *result, *tmp;
    gint i;

    if (G_UNLIKELY(wd == NULL))
        return g_strdup("No weather data.");

    if (G_UNLIKELY(wd->timeslices == NULL))
        return g_strdup("Weather data: No timeslices available.");

    out = g_string_sized_new(20480);
    g_string_assign(out, "Timeslices (local time): ");
    g_string_append_printf(out, "%d timeslices available.\n",
                           wd->timeslices->len);
    for (i = 0; i < wd->timeslices->len; i++) {
        timeslice = g_array_index(wd->timeslices, xml_time *, i);
        tmp = weather_dump_timeslice(timeslice);
        g_string_append_printf(out, "  #%3d: %s", i + 1, tmp);
        g_free(tmp);
    }

    /* Remove trailing newline */
    if (out->str[out->len - 1] == '\n')
        out->str[--out->len] = '\0';

    /* Free GString only and return its character data */
    result = out->str;
    g_string_free(out, FALSE);
    return result;
}


gchar *
weather_dump_plugindata(const plugin_data *data)
{
    GString *out;
    gchar *last_astro_update, *last_weather_update, *last_conditions_update;
    gchar *next_astro_update, *next_weather_update, *next_conditions_update;
    gchar *next_wakeup, *result;

    last_astro_update = format_date(data->astro_update->last, "%c", TRUE);
    last_weather_update = format_date(data->weather_update->last, "%c", TRUE);
    last_conditions_update =
        format_date(data->conditions_update->last, "%c", TRUE);
    next_astro_update = format_date(data->astro_update->next, "%c", TRUE);
    next_weather_update = format_date(data->weather_update->next, "%c", TRUE);
    next_conditions_update =
        format_date(data->conditions_update->next, "%c", TRUE);
    next_wakeup = format_date(data->next_wakeup, "%c", TRUE);

    out = g_string_sized_new(1024);
    g_string_assign(out, "xfce_weatherdata:\n");
    g_string_append_printf(out,
                           "  --------------------------------------------\n"
                           "  panel size: %d px\n"
                           "  panel rows: %d px\n"
                           "  single row: %s\n"
                           "  panel orientation: %d\n"
                           "  --------------------------------------------\n"
#ifdef HAVE_UPOWER_GLIB
                           "  upower on battery: %s\n"
#endif
                           "  power saving: %s\n"
                           "  --------------------------------------------\n"
                           "  last astro update: %s\n"
                           "  next astro update: %s\n"
                           "  astro download attempts: %d\n"
                           "  last weather update: %s\n"
                           "  next weather update: %s\n"
                           "  weather download attempts: %d\n"
                           "  last conditions update: %s\n"
                           "  next conditions update: %s\n"
                           "  next scheduled wakeup: %s\n"
                           "  wakeup reason: %s\n"
                           "  --------------------------------------------\n"
                           "  geonames username set by user: %s\n"
                           "  --------------------------------------------\n"
                           "  location name: %s\n"
                           "  latitude: %s\n"
                           "  longitude: %s\n"
                           "  msl: %d\n"
                           "  timezone: %s\n"
                           "  initial timezone: %s\n"
                           "  night time: %s\n"
                           "  --------------------------------------------\n"
                           "  icon theme dir: %s\n"
                           "  tooltip style: %d\n"
                           "  forecast layout: %d\n"
                           "  forecast days: %d\n"
                           "  round values: %s\n"
                           "  --------------------------------------------\n"
                           "  show scrollbox: %s\n"
                           "  scrollbox lines: %d\n"
                           "  scrollbox font: %s\n"
                           "  scrollbox color: %s\n"
                           "  scrollbox use color: %s\n"
                           "  animate scrollbox: %s\n"
                           "  --------------------------------------------",
                           data->panel_size,
                           data->panel_rows,
                           YESNO(data->single_row),
                           data->panel_orientation,
#ifdef HAVE_UPOWER_GLIB
                           YESNO(data->upower_on_battery),
#endif
                           YESNO(data->power_saving),
                           last_astro_update,
                           next_astro_update,
                           data->astro_update->attempt,
                           last_weather_update,
                           next_weather_update,
                           data->weather_update->attempt,
                           last_conditions_update,
                           next_conditions_update,
                           next_wakeup,
                           data->next_wakeup_reason,
                           YESNO(data->geonames_username),
                           data->location_name,
                           data->lat,
                           data->lon,
                           data->msl,
                           data->timezone,
                           data->timezone_initial,
                           YESNO(data->night_time),
                           (data->icon_theme) ? (data->icon_theme->dir) : NULL,
                           data->tooltip_style,
                           data->forecast_layout,
                           data->forecast_days,
                           YESNO(data->round),
                           YESNO(data->show_scrollbox),
                           data->scrollbox_lines,
                           data->scrollbox_font,
                           gdk_color_to_string(&(data->scrollbox_color)),
                           YESNO(data->scrollbox_use_color),
                           YESNO(data->scrollbox_animate));
    g_free(next_wakeup);
    g_free(next_astro_update);
    g_free(next_weather_update);
    g_free(next_conditions_update);
    g_free(last_astro_update);
    g_free(last_weather_update);
    g_free(last_conditions_update);

    /* Free GString only and return its character data */
    result = out->str;
    g_string_free(out, FALSE);
    return result;
}
