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

#include <libxfce4util/libxfce4util.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"
#include "weather-debug.h"

/* fallback values when astrodata is unavailable */
#define NIGHT_TIME_START 21
#define NIGHT_TIME_END 5


#define CHK_NULL(s) ((s) ? g_strdup(s) : g_strdup(""))

#define ROUND_TO_INT(default_format) (round ? "%.0f" : default_format)

#define LOCALE_DOUBLE(value, format)                \
    (g_strdup_printf(format,                        \
                     g_ascii_strtod(value, NULL)))

#define INTERPOLATE_OR_COPY(var)                                        \
    if (ipol)                                                           \
        comb->location->var =                                           \
            interpolate_gchar_value(start->location->var,               \
                                    end->location->var,                 \
                                    comb->start, comb->end,             \
                                    comb->point);                       \
    else                                                                \
        comb->location->var = g_strdup(end->location->var);

#define COMB_END_COPY(var)                              \
    comb->location->var = g_strdup(end->location->var);


static gboolean
has_timeslice(xml_weather *data,
              const time_t start_t,
              const time_t end_t)
{
    guint i = 0;

    for (i = 0; i < data->num_timeslices; i++)
        if (data->timeslice[i]->start == start_t &&
            data->timeslice[i]->end == end_t)
            return TRUE;
    return FALSE;
}


/* convert string to a double value, returning backup value on error */
gdouble
string_to_double(const gchar *str,
                 gdouble backup)
{
    gdouble d = backup;
    if (str && strlen(str) > 0)
        d = g_ascii_strtod(str, NULL);
    return d;
}


gchar *
get_data(const xml_time *timeslice,
         const units_config *units,
         const data_types type,
         gboolean round)
{
    const xml_location *loc = NULL;
    gdouble val;

    if (timeslice == NULL || timeslice->location == NULL || units == NULL)
        return g_strdup("");

    loc = timeslice->location;

    switch (type) {
    case ALTITUDE:
        switch (units->altitude) {
        case METERS:
            return LOCALE_DOUBLE(loc->altitude, "%.0f");

        case FEET:
            val = string_to_double(loc->altitude, 0);
            val /= 0.3048;
            return g_strdup_printf(ROUND_TO_INT("%.2f"), val);
        }

    case LATITUDE:
        return LOCALE_DOUBLE(loc->latitude, "%.4f");

    case LONGITUDE:
        return LOCALE_DOUBLE(loc->longitude, "%.4f");

    case TEMPERATURE:      /* source may be in Celsius or Fahrenheit */
        val = string_to_double(loc->temperature_value, 0);
        if (units->temperature == FAHRENHEIT &&
            (!strcmp(loc->temperature_unit, "celcius") ||
             !strcmp(loc->temperature_unit, "celsius")))
            val = val * 9.0 / 5.0 + 32.0;
        else if (units->temperature == CELSIUS &&
                 !strcmp(loc->temperature_unit, "fahrenheit"))
            val = (val - 32.0) * 5.0 / 9.0;
        return g_strdup_printf(ROUND_TO_INT("%.1f"), val);

    case PRESSURE:         /* source is in hectopascals */
        val = string_to_double(loc->pressure_value, 0);
        switch (units->pressure) {
        case INCH_MERCURY:
            val *= 0.03;
            break;
        case PSI:
            val *= 0.01450378911491;
            break;
        case TORR:
            val /= 1.333224;
            break;
        }
        return g_strdup_printf(ROUND_TO_INT("%.1f"), val);

    case WIND_SPEED:       /* source is in meters per hour */
        val = string_to_double(loc->wind_speed_mps, 0);
        switch (units->windspeed) {
        case KMH:
            val *= 3.6;
            break;
        case MPH:
            val *= 2.2369362920544;
            break;
        }
        return g_strdup_printf(ROUND_TO_INT("%.1f"), val);

    case WIND_BEAUFORT:
        val = string_to_double(loc->wind_speed_beaufort, 0);
        return g_strdup_printf("%.0f", val);

    case WIND_DIRECTION:
        return CHK_NULL(loc->wind_dir_name);

    case WIND_DIRECTION_DEG:
        return LOCALE_DOUBLE(loc->wind_dir_deg, ROUND_TO_INT("%.1f"));

    case HUMIDITY:
        return LOCALE_DOUBLE(loc->humidity_value, ROUND_TO_INT("%.1f"));

    case CLOUDS_LOW:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_LOW],
                             ROUND_TO_INT("%.1f"));

    case CLOUDS_MED:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_MED],
                             ROUND_TO_INT("%.1f"));

    case CLOUDS_HIGH:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_HIGH],
                             ROUND_TO_INT("%.1f"));

    case CLOUDINESS:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_CLOUDINESS],
                             ROUND_TO_INT("%.1f"));

    case FOG:
        return LOCALE_DOUBLE(loc->fog_percent, ROUND_TO_INT("%.1f"));

    case PRECIPITATIONS:   /* source is in millimeters */
        val = string_to_double(loc->precipitation_value, 0);
        if (units->precipitations == INCHES)
            val /= 25.4;
        return g_strdup_printf(ROUND_TO_INT("%.1f"), val);

    case SYMBOL:
        return CHK_NULL(loc->symbol);
    }

    return g_strdup("");
}


const gchar *
get_unit(const units_config *units,
         const data_types type)
{
    if (units == NULL)
        return "";

    switch (type) {
    case ALTITUDE:
        return (units->altitude == FEET) ? _("ft") : _("m");
    case TEMPERATURE:
        return (units->temperature == FAHRENHEIT) ? _("°F") : _("°C");
    case PRESSURE:
        switch (units->pressure) {
        case HECTOPASCAL:
            return _("hPa");
        case INCH_MERCURY:
            return _("inHg");
        case PSI:
            return _("psi");
        case TORR:
            return _("mmHg");
        }
    case WIND_SPEED:
        switch (units->windspeed) {
        case KMH:
            return _("km/h");
        case MPH:
            return _("mph");
        case MPS:
            return _("m/s");
        }
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
        return (units->precipitations == INCHES) ? _("in") : _("mm");
    case SYMBOL:
    case WIND_BEAUFORT:
    case WIND_DIRECTION:
        return "";
    }
    return "";
}


/*
 * Find out whether it's night or day.
 *
 * Either use the exact times for sunrise and sunset if
 * available, or fallback to reasonable arbitrary values.
 */
gboolean
is_night_time(const xml_astro *astro)
{
    time_t now_t;
    struct tm now_tm;

    time(&now_t);

    if (G_LIKELY(astro)) {
        /* Polar night */
        if (astro->sun_never_rises)
            return TRUE;

        /* Polar day */
        if (astro->sun_never_sets)
            return FALSE;

        /* Sunrise and sunset are known */
        if (difftime(astro->sunrise, now_t) >= 0)
            return TRUE;

        if (difftime(astro->sunset, now_t) <= 0)
            return TRUE;

        return FALSE;
    }

    /* no astrodata available, use fallback values */
    now_tm = *localtime(&now_t);
    return (now_tm.tm_hour >= NIGHT_TIME_START ||
            now_tm.tm_hour < NIGHT_TIME_END);
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
static void
get_daytime_interval(struct tm *start_tm,
                     struct tm *end_tm,
                     const daytime dt)
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


/*
 * Return wind direction name (S, N, W,...) for wind degrees.
 */
static gchar*
wind_dir_name_by_deg(gchar *degrees, gboolean long_name)
{
    gdouble deg;

    if (G_UNLIKELY(degrees == NULL))
        return "";

    deg = string_to_double(degrees, 0);

    if (deg >= 360 - 22.5 && deg < 45 - 22.5)
        return (long_name) ? _("North") : _("N");

    if (deg >= 45 - 22.5 && deg < 45 + 22.5)
        return (long_name) ? _("North-East") : _("NE");

    if (deg >= 90 - 22.5 && deg < 90 + 22.5)
        return (long_name) ? _("East") : _("E");

    if (deg >= 135 - 22.5 && deg < 135 + 22.5)
        return (long_name) ? _("South-East") : _("SE");

    if (deg >= 180 - 22.5 && deg < 180 + 22.5)
        return (long_name) ? _("South") : _("S");

    if (deg >= 225 - 22.5 && deg < 225 + 22.5)
        return (long_name) ? _("South-West") : _("SW");

    if (deg >= 270 - 22.5 && deg < 270 + 22.5)
        return (long_name) ? _("West") : _("W");

    if (deg >= 315 - 22.5 && deg < 315 + 22.5)
        return (long_name) ? _("North-West") : _("NW");

    return "";
}


/*
 * Interpolate data for a certain time in a given interval
 */
static gdouble
interpolate_value(gdouble value_start,
                  gdouble value_end,
                  time_t start_t,
                  time_t end_t,
                  time_t between_t)
{
    gdouble total, part, ratio, delta, result;

    /* calculate durations from start to end and start to between */
    total = difftime(end_t, start_t);
    part = difftime(between_t, start_t);

    /* calculate ratio of these durations */
    ratio = part / total;

    /* now how big is that change? */
    delta = (value_end - value_start) * ratio;

    /* apply change and return corresponding value for between_t */
    result = value_start + delta;
    return result;
}


/*
 * convert gchar in a gdouble and interpolate the value
 */
static gchar *
interpolate_gchar_value(gchar *value_start,
                        gchar *value_end,
                        time_t start_t,
                        time_t end_t,
                        time_t between_t)
{
    gchar value_result[10];
    gdouble val_start, val_end, val_result;

    if (value_end == NULL)
        return NULL;

    if (value_start == NULL)
        return g_strdup(value_end);

    val_start = string_to_double(value_start, 0);
    val_end = string_to_double(value_end, 0);

    val_result = interpolate_value(val_start, val_end,
                                   start_t, end_t, between_t);
    weather_debug("Interpolated data: start=%f, end=%f, result=%f",
                  val_start, val_end, val_result);
    (void) g_ascii_formatd(value_result, 10, "%.1f", val_result);

    return g_strdup(value_result);
}


/* Create a new combined timeslice, with optionally interpolated data */
static xml_time *
make_combined_timeslice(xml_weather *data,
                        const xml_time *interval,
                        const time_t *between_t)
{
    xml_time *comb, *start = NULL, *end = NULL;
    xml_location *combloc, *startloc, *endloc;
    gboolean ipol = (between_t != NULL) ? TRUE : FALSE;
    guint i;

    /* find point data at start of interval (may not be available) */
    if (has_timeslice(data, interval->start, interval->start))
        start = get_timeslice(data, interval->start, interval->start);

    /* find point interval at end of interval */
    if (has_timeslice(data, interval->end, interval->end))
        end = get_timeslice(data, interval->end, interval->end);

    if (start == NULL && end == NULL)
        return NULL;

    /* create new timeslice to hold our copy */
    comb = g_slice_new0(xml_time);
    if (comb == NULL)
        return NULL;

    comb->location = g_slice_new0(xml_location);
    if (comb->location == NULL) {
        g_slice_free(xml_time, comb);
        return NULL;
    }

    /* do not interpolate if no point data available at start of interval */
    if (start == NULL) {
        comb->point = end->start;
        start = end;
    } else if (between_t)          /* only interpolate if requested */
        comb->point = *between_t;

    comb->start = interval->start;
    comb->end = interval->end;

    COMB_END_COPY(altitude);
    COMB_END_COPY(latitude);
    COMB_END_COPY(longitude);

    INTERPOLATE_OR_COPY(temperature_value);
    COMB_END_COPY(temperature_unit);

    INTERPOLATE_OR_COPY(wind_dir_deg);
    comb->location->wind_dir_name =
        g_strdup(wind_dir_name_by_deg(comb->location->wind_dir_deg, FALSE));

    INTERPOLATE_OR_COPY(wind_speed_mps);
    INTERPOLATE_OR_COPY(wind_speed_beaufort);
    INTERPOLATE_OR_COPY(humidity_value);
    COMB_END_COPY(humidity_unit);

    INTERPOLATE_OR_COPY(pressure_value);
    COMB_END_COPY(pressure_unit);

    for (i = 0; i < CLOUDS_PERC_NUM; i++)
        INTERPOLATE_OR_COPY(clouds_percent[i]);

    INTERPOLATE_OR_COPY(fog_percent);

    /* it makes no sense to interpolate the following (interval) values */
    comb->location->precipitation_value =
        g_strdup(interval->location->precipitation_value);
    comb->location->precipitation_unit =
        g_strdup(interval->location->precipitation_unit);

    comb->location->symbol_id = interval->location->symbol_id;
    comb->location->symbol = g_strdup(interval->location->symbol);

    return comb;
}


/* Return current weather conditions, or NULL if not available. */
xml_time *
get_current_conditions(const xml_weather *data)
{
    if (data == NULL)
        return NULL;
    return data->current_conditions;
}


time_t
time_calc(const struct tm time_tm,
          const gint year,
          const gint month,
          const gint day,
          const gint hour,
          const gint min,
          const gint sec)
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
time_calc_hour(const struct tm time_tm,
               const gint hours)
{
    return time_calc(time_tm, 0, 0, 0, hours, 0, 0);
}


time_t
time_calc_day(const struct tm time_tm,
              const gint days)
{
    return time_calc(time_tm, 0, 0, days, 0, 0, 0);
}


/*
 * Find timeslice of the given interval near start and end
 * times. Shift maximum prev_hours_limit hours into the past and
 * next_hours_limit hours into the future.
 */
static xml_time *
find_timeslice(xml_weather *data,
               struct tm start_tm,
               struct tm end_tm,
               const gint prev_hours_limit,
               const gint next_hours_limit)
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
static xml_time *
find_shortest_timeslice(xml_weather *data,
                        struct tm start_tm,
                        struct tm end_tm,
                        const gint prev_hours_limit,
                        const gint next_hours_limit,
                        const gint interval_limit)
{
    xml_time *interval_data;
    time_t start_t, end_t;
    gint interval;

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


xml_time *
make_current_conditions(xml_weather *data,
                        time_t now_t)
{
    xml_time *interval_data;
    struct tm now_tm, start_tm, end_tm;
    time_t end_t;

    /* now search for the nearest and shortest interval data
       available, using a maximum interval of 6 hours */
    now_tm = *localtime(&now_t);
    end_tm = start_tm = now_tm;

    /* minimum interval is 1 hour */
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

    /* create combined timeslice with interpolated point and interval data */
    return make_combined_timeslice(data, interval_data, &now_t);
}


/*
 * Get forecast data for a given daytime for the day (today + day).
 */
xml_time *
make_forecast_data(xml_weather *data,
                   const int day,
                   const daytime dt)
{
    xml_time *interval_data = NULL;
    struct tm start_tm, end_tm;
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

    /* create combined timeslice with non-interpolated point
       and interval data */
    return make_combined_timeslice(data, interval_data, NULL);
}
