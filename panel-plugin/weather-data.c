/*  Copyright (c) 2003-2007 Xfce Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#define CHK_NULL(s) ((s) ? g_strdup(s) : g_strdup(""))
#define LOCALE_DOUBLE(value, \
                      format) (g_strdup_printf(format, \
                                               g_ascii_strtod(value, NULL)))


gboolean
has_timeslice(xml_weather *data,
              time_t start_t,
              time_t end_t)
{
    int i = 0;
    for (i = 0; i < data->num_timeslices; i++)
        if (data->timeslice[i]->start == start_t &&
            data->timeslice[i]->end == end_t)
            return TRUE;
    return FALSE;
}


gchar *
get_data(xml_time *timeslice,
         unit_systems unit_system,
         datas type)
{
    const xml_location *loc = NULL;
    double val;

    if (timeslice == NULL)
        return g_strdup("");

    loc = timeslice->location;

    switch (type) {
    case ALTITUDE:
        if (unit_system == METRIC)
            return LOCALE_DOUBLE(loc->altitude, "%.0f");
        val = g_ascii_strtod(loc->altitude, NULL);
        val /= 0.3048;
        return g_strdup_printf("%.2f", val);
    case LATITUDE:
        return LOCALE_DOUBLE(loc->latitude, "%.4f");
    case LONGITUDE:
        return LOCALE_DOUBLE(loc->longitude, "%.4f");
    case TEMPERATURE:
        val = g_ascii_strtod(loc->temperature_value, NULL);
        if (unit_system == IMPERIAL &&
            (!strcmp(loc->temperature_unit, "celcius") ||
             !strcmp(loc->temperature_unit, "celsius")))
            val = val * 9.0 / 5.0 + 32.0;
        else if (unit_system == METRIC &&
                 !strcmp(loc->temperature_unit, "fahrenheit"))
            val = (val - 32.0) * 5.0 / 9.0;
        return g_strdup_printf("%.1f", val);
    case PRESSURE:
        if (unit_system == METRIC)
            return LOCALE_DOUBLE(loc->pressure_value, "%.1f");
        val = g_ascii_strtod(loc->pressure_value, NULL);
        if (unit_system == IMPERIAL)
            val *= 0.01450378911491;
        return g_strdup_printf("%.1f", val);
    case WIND_SPEED:
        val = g_ascii_strtod(loc->wind_speed_mps, NULL);
        if (unit_system == IMPERIAL)
            val *= 2.2369362920544;
        else if (unit_system == METRIC)
            val *= 3.6;
        return g_strdup_printf("%.1f", val);
    case WIND_BEAUFORT:
        return CHK_NULL(loc->wind_speed_beaufort);
    case WIND_DIRECTION:
        return CHK_NULL(loc->wind_dir_name);
    case WIND_DIRECTION_DEG:
        return LOCALE_DOUBLE(loc->wind_dir_deg, "%.1f");
    case HUMIDITY:
        return LOCALE_DOUBLE(loc->humidity_value, "%.1f");
    case CLOUDS_LOW:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_LOW], "%.1f");
    case CLOUDS_MED:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_MED], "%.1f");
    case CLOUDS_HIGH:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_HIGH], "%.1f");
    case CLOUDINESS:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_CLOUDINESS],
                             "%.1f");
    case FOG:
        return LOCALE_DOUBLE(loc->fog_percent, "%.1f");
    case PRECIPITATIONS:
        if (unit_system == METRIC)
            return LOCALE_DOUBLE(loc->precipitation_value, "%.1f");
        val = g_ascii_strtod(loc->precipitation_value, NULL);
        if (unit_system == IMPERIAL)
            val /= 25.4;
        return g_strdup_printf("%.3f", val);
    case SYMBOL:
        return CHK_NULL(loc->symbol);
    }
    return g_strdup("");
}


const gchar *
get_unit(unit_systems unit_system,
         datas type)
{
    switch (type) {
    case ALTITUDE:
        return (unit_system == IMPERIAL) ? _("ft") : _("m");
    case TEMPERATURE:
        return (unit_system == IMPERIAL) ? _("°F") : _("°C");
    case PRESSURE:
        return (unit_system == IMPERIAL) ? _("psi") : _("hPa");
    case WIND_SPEED:
        return (unit_system == IMPERIAL) ? _("mph") : _("km/h");
    case WIND_DIRECTION_DEG:
    case LATITUDE:
    case LONGITUDE:
        return "°";
    case HUMIDITY:
    case CLOUDS_LOW:
    case CLOUDS_MED:
    case CLOUDS_HIGH:
    case CLOUDINESS:
    case FOG:
        return "%";
    case PRECIPITATIONS:
        return (unit_system == IMPERIAL) ? _("in") : _("mm");
    case SYMBOL:
    case WIND_BEAUFORT:
    case WIND_DIRECTION:
        return "";
    }
    return "";
}


/*
 * Calculate start and end of a daytime interval using given dates.
 * We ought to take one of the intervals supplied by the XML feed,
 * which gives the most consistent data and does not force too many
 * searches to find something that fits. The following chosen
 * intervals were pretty reliable for several tested locations at the
 * time of this writing and gave reasonable results:
 *   Morning:   08:00-14:00
 *   Afternoon: 14:00-20:00
 *   Evening:   20:00-02:00
 *   Night:     02:00-08:00
 */
void
get_daytime_interval(struct tm *start_tm,
                     struct tm *end_tm,
                     daytime dt)
{
    start_tm->tm_min = end_tm->tm_min = 0;
    start_tm->tm_sec = end_tm->tm_sec = 0;
    start_tm->tm_isdst = end_tm->tm_isdst = -1;
    switch (dt) {
    case MORNING:
        start_tm->tm_hour = 8;
        end_tm->tm_hour = 14;
        break;
    case AFTERNOON:
        start_tm->tm_hour = 14;
        end_tm->tm_hour = 20;
        break;
    case EVENING:
        start_tm->tm_hour = 20;
        end_tm->tm_hour = 26;
        break;
    case NIGHT:
        start_tm->tm_hour = 26;
        end_tm->tm_hour = 32;
        break;
    }
}


/* Return current weather conditions, or NULL if not available. */
xml_time *
get_current_conditions(xml_weather *data)
{
    if (data == NULL)
        return NULL;
    return data->current_conditions;
}


/*
 * Check whether it is night or day.
 *
 * FIXME: Until we have a way to get the exact times for sunrise and
 * sunset, we'll have to use reasonable hardcoded values.
 */
gboolean
is_night_time()
{
    time_t now_t;
    struct tm now_tm;

    time(&now_t);
    now_tm = *localtime(&now_t);
    return (now_tm.tm_hour >= 21 || now_tm.tm_hour < 5);
}


time_t
time_calc(struct tm time_tm,
          gint year,
          gint month,
          gint day,
          gint hour,
          gint min,
          gint sec)
{
    time_t result;
    struct tm new_tm;

    new_tm = time_tm;
    new_tm.tm_isdst = -1;
    if (year)
        new_tm.tm_year += year;
    if (month)
        new_tm.tm_mon += month;
    if (day)
        new_tm.tm_mday += day;
    if (hour)
        new_tm.tm_hour += hour;
    if (min)
        new_tm.tm_min += min;
    if (sec)
        new_tm.tm_sec += sec;
    result = mktime(&new_tm);
    return result;
}


time_t
time_calc_hour(struct tm time_tm,
               gint hours)
{
    return time_calc(time_tm, 0, 0, 0, hours, 0, 0);
}


time_t
time_calc_day(struct tm time_tm,
              gint days)
{
    return time_calc(time_tm, 0, 0, days, 0, 0, 0);
}


/*
 * Find timeslice of the given interval near start and end
 * times. Shift maximum prev_hours_limit hours into the past and
 * next_hours_limit hours into the future.
 */
xml_time *
find_timeslice(xml_weather *data,
               struct tm start_tm,
               struct tm end_tm,
               gint prev_hours_limit,
               gint next_hours_limit)
{
    time_t start_t, end_t;
    gint hours = 0;

    /* set start and end times to the exact hour */
    end_tm.tm_min = start_tm.tm_min = 0;
    end_tm.tm_sec = start_tm.tm_sec = 0;

    while (hours >= prev_hours_limit && hours <= next_hours_limit) {
        /* check previous hours */
        if ((0 - hours) >= prev_hours_limit) {
            start_t = time_calc_hour(start_tm, 0 - hours);
            end_t = time_calc_hour(end_tm, 0 - hours);

            if (has_timeslice(data, start_t, end_t))
                return get_timeslice(data, start_t, end_t);
        }

        /* check later hours */
        if (hours != 0 && hours <= next_hours_limit) {
            start_t = time_calc_hour(start_tm, hours);
            end_t = time_calc_hour(end_tm, hours);

            if (has_timeslice(data, start_t, end_t))
                return get_timeslice(data, start_t, end_t);
        }
        hours++;
    }
    return NULL;
}


/*
 * Find the timeslice with the shortest interval near the given start
 * and end times
 */
xml_time *
find_shortest_timeslice(xml_weather *data,
                        struct tm start_tm,
                        struct tm end_tm,
                        gint prev_hours_limit,
                        gint next_hours_limit,
                        gint interval_limit)
{
    xml_time *interval_data;
    time_t start_t, end_t;
    gint hours, interval;

    /* set start and end times to the exact hour */
    end_tm.tm_min = start_tm.tm_min = 0;
    end_tm.tm_sec = start_tm.tm_sec = 0;

    start_t = mktime(&start_tm);
    end_t = mktime(&end_tm);

    start_tm = *localtime(&start_t);
    end_tm = *localtime(&end_t);

    /* minimum interval is provided by start_tm and end_tm */
    interval = (gint) (difftime(end_t, start_t) / 3600);

    while (interval <= interval_limit) {
        interval_data = find_timeslice(data, start_tm, end_tm,
                                       prev_hours_limit, next_hours_limit);
        if (interval_data != NULL)
            return interval_data;

        interval++;
        start_t = mktime(&start_tm);
        end_t = time_calc_hour(end_tm, interval);
        start_tm = *localtime(&start_t);
        end_tm = *localtime(&end_t);
    }

    return NULL;
}


/*
 * Take point and interval data and generate one combined timeslice
 * that provides all information needed to present a forecast.
 */
xml_time *
make_combined_timeslice(xml_time *point,
                        xml_time *interval)
{
    xml_time *forecast;
    xml_location *loc;
    gint i;

    if (point == NULL || interval == NULL)
        return NULL;

    forecast = g_slice_new0(xml_time);
    if (forecast == NULL)
        return NULL;

    loc = g_slice_new0(xml_location);
    if (loc == NULL)
        return forecast;

    forecast->point = point->start;
    forecast->start = interval->start;
    forecast->end = interval->end;

    loc->altitude = g_strdup(point->location->altitude);
    loc->latitude = g_strdup(point->location->latitude);
    loc->longitude = g_strdup(point->location->longitude);

    loc->temperature_value = g_strdup(point->location->temperature_value);
    loc->temperature_unit = g_strdup(point->location->temperature_unit);

    loc->wind_dir_deg = g_strdup(point->location->wind_dir_deg);
    loc->wind_dir_name = g_strdup(point->location->wind_dir_name);
    loc->wind_speed_mps = g_strdup(point->location->wind_speed_mps);
    loc->wind_speed_beaufort = g_strdup(point->location->wind_speed_beaufort);

    loc->humidity_value = g_strdup(point->location->humidity_value);
    loc->humidity_unit = g_strdup(point->location->humidity_unit);

    loc->pressure_value = g_strdup(point->location->pressure_value);
    loc->pressure_unit = g_strdup(point->location->pressure_unit);

    for (i = 0; i < CLOUDS_PERC_NUM; i++)
        loc->clouds_percent[i] = g_strdup(point->location->clouds_percent[i]);

    loc->fog_percent = g_strdup(point->location->fog_percent);

    loc->precipitation_value =
        g_strdup(interval->location->precipitation_value);
    loc->precipitation_unit = g_strdup(interval->location->precipitation_unit);

    loc->symbol_id = interval->location->symbol_id;
    loc->symbol = g_strdup(interval->location->symbol);

    forecast->location = loc;

    return forecast;
}


xml_time *
make_current_conditions(xml_weather *data)
{
    xml_time *conditions, *point_data, *interval_data;
    struct tm now_tm, start_tm, end_tm;
    time_t now_t, start_t, end_t;
    gint interval;

    /* get the current time */
    time(&now_t);
    now_tm = *localtime(&now_t);

    /* find nearest point data, starting with the current hour, with a
       deviation of 1 hour into the past and 6 hours into the future */
    point_data = find_timeslice(data, now_tm, now_tm, -1, 6);
    if (point_data == NULL)
        return NULL;

    /* now search for the nearest and shortest interval data
       available, using a maximum interval of 6 hours */
    end_tm = start_tm = now_tm;

    /* set interval to 1 hour as minimum, we don't want to retrieve
       point data */
    end_t = time_calc_hour(end_tm, 1);
    end_tm = *localtime(&end_t);

    /* We want to keep the hour deviation as small as possible,
       so let's try an interval with ±1 hour deviation first */
    interval_data = find_shortest_timeslice(data, start_tm, end_tm, -1, 1, 6);
    if (interval_data == NULL) {
        /* in case we were unsuccessful we might need to enlarge the
           search radius */
        interval_data = find_shortest_timeslice(data, start_tm, end_tm,
                                                -3, 3, 6);
        if (interval_data == NULL)
            /* and maybe it's necessary to try even harder... */
            interval_data = find_shortest_timeslice(data, start_tm, end_tm,
                                                    -3, 6, 6);
    }
    if (interval_data == NULL)
        return NULL;

    /* create a new timeslice with combined point and interval data */
    conditions = make_combined_timeslice(point_data, interval_data);
    return conditions;
}


/*
 * Get forecast data for a given daytime for the day (today + day).
 */
xml_time *
make_forecast_data(xml_weather *data,
                   int day,
                   daytime dt)
{
    xml_time *forecast = NULL, *point_data = NULL, *interval_data = NULL;
    struct tm now_tm, start_tm, end_tm;
    time_t now_t, start_t, end_t;
    gint interval;

    /* initialize times to the current day */
    time(&now_t);
    start_tm = *localtime(&now_t);
    end_tm = *localtime(&now_t);

    /* calculate daytime interval start and end times for the requested day */
    start_tm.tm_mday += day;
    end_tm.tm_mday += day;
    get_daytime_interval(&start_tm, &end_tm, dt);
    start_t = mktime(&start_tm);
    end_t = mktime(&end_tm);

    /* find point data using a maximum variance of ±3 hours*/
    point_data = find_timeslice(data, start_tm, start_tm, -3, 3);
    if (point_data == NULL)
        return NULL;

    /* next find biggest possible (limited by daytime) interval data
       using a maximum deviation of ±3 hours */
    while ((interval = (gint) (difftime(end_t, start_t) / 3600)) > 0) {
        interval_data = find_timeslice(data, start_tm, end_tm, -3, 3);
        if (interval_data != NULL)
            break;
        end_t = time_calc_hour(end_tm, -1);
        end_tm = *localtime(&end_t);
    }
    if (interval_data == NULL)
        return NULL;

    /* create a new timeslice with combined point and interval data */
    forecast = make_combined_timeslice(point_data, interval_data);
    return forecast;
}
