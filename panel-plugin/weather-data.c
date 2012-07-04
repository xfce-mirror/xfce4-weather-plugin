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

#define CHK_NULL(s) ((s) ? g_strdup(s):g_strdup(""))
#define LOCALE_DOUBLE(value, format) (g_strdup_printf(format, g_ascii_strtod(value, NULL)))

gboolean has_timeslice(xml_weather *data, time_t start, time_t end)
{
    int i = 0;
	for (i = 0; i < data->num_timeslices; i++) {
        if (data->timeslice[i]->start == start
			&& data->timeslice[i]->end == end)
            return TRUE;
	}
    return FALSE;
}

gchar *
get_data (xml_time *timeslice, units unit, datas type)
{
	const xml_location *loc = NULL;
	double val;

	if (timeslice == NULL)
		return g_strdup("");

	loc = timeslice->location;

	switch(type) {
	case ALTITUDE:
		return CHK_NULL(loc->altitude);
	case LATITUDE:
		return LOCALE_DOUBLE(loc->latitude, "%.4f");
	case LONGITUDE:
		return LOCALE_DOUBLE(loc->longitude, "%.4f");
	case TEMPERATURE:
		val = g_ascii_strtod(loc->temperature_value, NULL);
		if (unit == IMPERIAL
			&& (strcmp(loc->temperature_unit, "celcius") == 0
				|| strcmp(loc->temperature_unit, "celsius" == 0)))
			val = val * 9.0 / 5.0 + 32.0;
		else if (unit == METRIC
				 && strcmp(loc->temperature_unit, "fahrenheit") == 0)
			val = (val - 32.0) * 5.0 / 9.0;
		return g_strdup_printf ("%.1f", val);
	case PRESSURE:
		return LOCALE_DOUBLE(loc->pressure_value, "%.1f");
	case WIND_SPEED:
		return LOCALE_DOUBLE(loc->wind_speed_mps, "%.1f");
	case WIND_BEAUFORT:
		return CHK_NULL(loc->wind_speed_beaufort);
	case WIND_DIRECTION:
		return CHK_NULL(loc->wind_dir_name);
	case WIND_DIRECTION_DEG:
		return LOCALE_DOUBLE(loc->wind_dir_deg, "%.1f");
	case HUMIDITY:
		return LOCALE_DOUBLE(loc->humidity_value, "%.1f");
	case CLOUDINESS_LOW:
		return LOCALE_DOUBLE(loc->cloudiness_percent[CLOUD_LOW], "%.1f");
	case CLOUDINESS_MED:
		return LOCALE_DOUBLE(loc->cloudiness_percent[CLOUD_MED], "%.1f");
	case CLOUDINESS_HIGH:
		return LOCALE_DOUBLE(loc->cloudiness_percent[CLOUD_HIGH], "%.1f");
	case CLOUDINESS_OVERALL:
		return LOCALE_DOUBLE(loc->cloudiness_percent[CLOUD_OVERALL], "%.1f");
	case FOG:
		return LOCALE_DOUBLE(loc->fog_percent, "%.1f");
	case PRECIPITATIONS:
		return LOCALE_DOUBLE(loc->precipitation_value, "%.1f");
	case SYMBOL:
		return CHK_NULL(loc->symbol);
	}
	return g_strdup("");
}

const gchar *
get_unit (xml_time *timeslice, units unit, datas type)
{
	const xml_location *loc = NULL;

	if (timeslice == NULL)
		return "";

	loc = timeslice->location;

	switch(type) {
	case ALTITUDE:
		return "m";
	case TEMPERATURE:
		return (unit == IMPERIAL) ? _("°F") : _("°C");
	case PRESSURE:
		return (loc->pressure_unit) ? loc->pressure_unit : "";
	case WIND_SPEED:
		return "m/s";
	case WIND_DIRECTION_DEG:
	case LATITUDE:
	case LONGITUDE:
		return "°";
	case HUMIDITY:
	case CLOUDINESS_LOW:
	case CLOUDINESS_MED:
	case CLOUDINESS_HIGH:
	case CLOUDINESS_OVERALL:
	case FOG:
		return "%";
	case PRECIPITATIONS:
		return "mm";
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
get_daytime_interval(struct tm *start_t, struct tm *end_t, daytime dt)
{
    start_t->tm_min = end_t->tm_min = 0;
    start_t->tm_sec = end_t->tm_sec = 0;
    start_t->tm_isdst = end_t->tm_isdst = -1;
	switch(dt) {
	case MORNING:
		start_t->tm_hour = 8;
		end_t->tm_hour = 14;
		break;
	case AFTERNOON:
		start_t->tm_hour = 14;
		end_t->tm_hour = 20;
		break;
	case EVENING:
		start_t->tm_hour = 20;
		end_t->tm_hour = 26;
		break;
	case NIGHT:
		start_t->tm_hour = 26;
		end_t->tm_hour = 32;
		break;
	}
}

/*
 * Check whether it is night or day. Until we have a way to get the
 * exact times for sunrise and sunset, we'll have to use reasonable
 * hardcoded values.
 */
gboolean
is_night_time()
{
    time_t now;
    struct tm tm_now;
    time(&now);
    tm_now = *localtime(&now);
    return (tm_now.tm_hour >= 21 || tm_now.tm_hour < 5);
}

time_t
time_calc(struct tm tm_time, gint year, gint month, gint day, gint hour, gint min, gint sec)
{
    time_t result;
    struct tm tm_new;
    tm_new = tm_time;
    if (year)
        tm_new.tm_year += year;
    if (month)
        tm_new.tm_mon += month;
    if (day)
        tm_new.tm_mday += day;
    if (hour)
        tm_new.tm_hour += hour;
    if (min)
        tm_new.tm_min += min;
    if (sec)
        tm_new.tm_sec += sec;
    result = mktime(&tm_new);
    return result;
}

time_t
time_calc_hour(struct tm tm_time, gint hours) {
    return time_calc(tm_time, 0, 0, 0, hours, 0, 0);
}

time_t
time_calc_day(struct tm tm_time, gint days) {
    return time_calc(tm_time, 0, 0, days, 0, 0, 0);
}

/*
 * Find a timeslice that best matches the start and end times within
 * reasonable limits.
 */
xml_time *
find_timeslice(xml_weather *data, struct tm tm_start, struct tm tm_end)
{
    time_t start_t, end_t;
    gint hours, hours_limit = 3, interval = 0, interval_limit;

    /* first search for intervals of the same length */
    start_t = mktime(&tm_start);
    end_t = mktime(&tm_end);
    interval_limit = (gint) (difftime(end_t, start_t) / 3600);

    while (interval <= interval_limit) {
        hours = 0;
        while (hours <= hours_limit) {
            /* check with previous hours */
            start_t = time_calc_hour(tm_start, 0 - hours);
            end_t = time_calc_hour(tm_end, 0 - hours - interval);

            if (has_timeslice(data, start_t, end_t))
                return get_timeslice(data, start_t, end_t);

            /* check with later hours */
            start_t = time_calc_hour(tm_start, hours);
            end_t = time_calc_hour(tm_end, hours - interval);

            if (has_timeslice(data, start_t, end_t))
                return get_timeslice(data, start_t, end_t);

            hours++;
        }
        interval++;
    }

    return NULL;
}

/*
 * Take point and interval data and generate one combined timeslice
 * that provides all information needed to present a forecast.
 */
xml_time *
make_combined_timeslice(xml_time *point, xml_time *interval)
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

    forecast->start = point->start;
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

    for (i = 0; i < NUM_CLOUDINESS; i++)
        loc->cloudiness_percent[i] = g_strdup(point->location->cloudiness_percent[i]);

    loc->fog_percent = g_strdup(point->location->fog_percent);

    loc->precipitation_value = g_strdup(interval->location->precipitation_value);
    loc->precipitation_unit = g_strdup(interval->location->precipitation_unit);

    loc->symbol_id = interval->location->symbol_id;
    loc->symbol = g_strdup(interval->location->symbol);

    forecast->location = loc;

    return forecast;
}

/*
 * Get forecast data for a given daytime for the day (today + day).
 */
xml_time *
make_forecast_data(xml_weather *data, int day, daytime dt)
{
	xml_time *forecast, *point, *interval;
	struct tm tm_now, tm_start, tm_end;
	time_t now, start_t, end_t;

	/* initialize times to the current day */
	time(&now);
	tm_start = *localtime(&now);
	tm_end = *localtime(&now);

	/* calculate daytime interval start and end times for the  requested day */
	tm_start.tm_mday += day;
	tm_end.tm_mday += day;
	get_daytime_interval(&tm_start, &tm_end, dt);
    start_t = mktime(&tm_start);
    end_t = mktime(&tm_end);

	/* find point data */
    point = find_timeslice(data, tm_start, tm_start);

	/* next find interval data */
    interval = find_timeslice(data, tm_start, tm_end);

    /* create a new timeslice with combined point and interval data */
    forecast = make_combined_timeslice(point, interval);
	return forecast;
}
