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

#define LOCALE_DOUBLE(value, format)                        \
    (value                                                  \
     ? g_strdup_printf(format, g_ascii_strtod(value, NULL)) \
     : g_strdup(""))

#define INTERPOLATE_OR_COPY(var, radian)                    \
    if (ipol)                                               \
        comb->location->var =                               \
            interpolate_gchar_value(start->location->var,   \
                                    end->location->var,     \
                                    comb->start, comb->end, \
                                    comb->point, radian);   \
    else                                                    \
        comb->location->var = g_strdup(end->location->var);

#define COMB_END_COPY(var)                              \
    comb->location->var = g_strdup(end->location->var);


/* struct to store results from searches for point data */
typedef struct {
    GArray *before;
    time_t point;
    GArray *after;
} point_data_results;


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


/* check whether timeslice is interval or point data */
gboolean
timeslice_is_interval(xml_time *timeslice)
{
    return (timeslice->location->symbol != NULL ||
            timeslice->location->precipitation_value != NULL);
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
        case KNOTS:
            val *= 1.9438445;
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
        case KNOTS:
            return _("kt");
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
 * Return wind direction name (S, N, W,...) for wind degrees.
 */
static gchar*
wind_dir_name_by_deg(gchar *degrees, gboolean long_name)
{
    gdouble deg;

    if (G_UNLIKELY(degrees == NULL))
        return "";

    deg = string_to_double(degrees, 0);

    if (deg >= 360 - 22.5 || deg < 45 - 22.5)
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
                        time_t between_t,
                        gboolean radian)
{
    gchar value_result[10];
    gdouble val_start, val_end, val_result;

    if (G_UNLIKELY(value_end == NULL))
        return NULL;

    if (value_start == NULL)
        return g_strdup(value_end);

    val_start = string_to_double(value_start, 0);
    val_end = string_to_double(value_end, 0);

    if (radian)
        if (val_end > val_start && val_end - val_start > 180)
            val_start += 360;
        else if (val_start > val_end && val_start - val_end > 180)
            val_end += 360;
    val_result = interpolate_value(val_start, val_end,
                                   start_t, end_t, between_t);
    if (radian && val_result >= 360)
        val_result -= 360;

    weather_debug("Interpolated data: start=%f, end=%f, result=%f",
                  val_start, val_end, val_result);
    (void) g_ascii_formatd(value_result, 10, "%.1f", val_result);

    return g_strdup(value_result);
}


/* Create a new combined timeslice, with optionally interpolated data */
static xml_time *
make_combined_timeslice(xml_weather *wd,
                        const xml_time *interval,
                        const time_t *between_t)
{
    xml_time *comb, *start, *end;
    gboolean ipol = (between_t != NULL) ? TRUE : FALSE;
    guint i;

    /* find point data at start of interval (may not be available) */
    start = get_timeslice(wd, interval->start, interval->start, NULL);

    /* find point interval at end of interval */
    end = get_timeslice(wd, interval->end, interval->end, NULL);

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
    } else if (ipol) {
        /* deal with timeslices that are in the near future and use point
           data available at the start of the interval */
        if (difftime(*between_t, start->start) <= 0)
            comb->point = start->start;
        else
            comb->point = *between_t;
    }

    comb->start = interval->start;
    comb->end = interval->end;

    COMB_END_COPY(altitude);
    COMB_END_COPY(latitude);
    COMB_END_COPY(longitude);

    INTERPOLATE_OR_COPY(temperature_value, FALSE);
    COMB_END_COPY(temperature_unit);

    INTERPOLATE_OR_COPY(wind_dir_deg, TRUE);
    comb->location->wind_dir_name =
        g_strdup(wind_dir_name_by_deg(comb->location->wind_dir_deg, FALSE));

    INTERPOLATE_OR_COPY(wind_speed_mps, FALSE);
    INTERPOLATE_OR_COPY(wind_speed_beaufort, FALSE);
    INTERPOLATE_OR_COPY(humidity_value, FALSE);
    COMB_END_COPY(humidity_unit);

    INTERPOLATE_OR_COPY(pressure_value, FALSE);
    COMB_END_COPY(pressure_unit);

    for (i = 0; i < CLOUDS_PERC_NUM; i++)
        INTERPOLATE_OR_COPY(clouds_percent[i], FALSE);

    INTERPOLATE_OR_COPY(fog_percent, FALSE);

    /* it makes no sense to interpolate the following (interval) values */
    comb->location->precipitation_value =
        g_strdup(interval->location->precipitation_value);
    comb->location->precipitation_unit =
        g_strdup(interval->location->precipitation_unit);

    comb->location->symbol_id = interval->location->symbol_id;
    comb->location->symbol = g_strdup(interval->location->symbol);

    return comb;
}


void
merge_timeslice(xml_weather *wd,
                const xml_time *timeslice)
{
    xml_time *old_ts, *new_ts;
    time_t now_t = time(NULL);
    guint i;

    g_assert(wd != NULL);
    if (G_UNLIKELY(wd == NULL))
        return;

    /* first check if it isn't too old */
    if (difftime(now_t, timeslice->end) > DATA_EXPIRY_TIME) {
        weather_debug("Not merging timeslice because it has expired.");
        return;
    }

    /* Copy timeslice, as it will be deleted by the calling function */
    new_ts = xml_time_copy(timeslice);

    /* check if there is a timeslice with the same interval and
       replace it with the current data */
    old_ts = get_timeslice(wd, timeslice->start, timeslice->end, &i);
    if (old_ts) {
        xml_time_free(old_ts);
        g_array_remove_index(wd->timeslices, i);
        g_array_insert_val(wd->timeslices, i, new_ts);
        weather_debug("Replaced existing timeslice at %d.", i);
    } else {
        g_array_prepend_val(wd->timeslices, new_ts);
        //weather_debug("Prepended timeslice to the existing timeslices.");
    }
}


/* Return current weather conditions, or NULL if not available. */
xml_time *
get_current_conditions(const xml_weather *wd)
{
    return wd ? wd->current_conditions : NULL;
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
find_timeslice(xml_weather *wd,
               struct tm start_tm,
               struct tm end_tm,
               const gint prev_hours_limit,
               const gint next_hours_limit)
{
    xml_time *timeslice;
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

            if ((timeslice = get_timeslice(wd, start_t, end_t, NULL)))
                return timeslice;
        }

        /* check later hours */
        if (hours != 0 && hours <= next_hours_limit) {
            start_t = time_calc_hour(start_tm, hours);
            end_t = time_calc_hour(end_tm, hours);

            if ((timeslice = get_timeslice(wd, start_t, end_t, NULL)))
                return timeslice;
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
find_shortest_timeslice(xml_weather *wd,
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
        interval_data = find_timeslice(wd, start_tm, end_tm,
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
 * Compare two xml_time structs using their start and end times,
 * returning the result as a qsort()-style comparison function (less
 * than zero for first arg is less than second arg, zero for equal,
 * greater zero if first arg is greater than second arg).
 */
gint
xml_time_compare(gpointer a,
                 gpointer b)
{
    xml_time *ts1 = (xml_time *) a;
    xml_time *ts2 = (xml_time *) b;
    gdouble diff;

    if (G_UNLIKELY(a == NULL && b == NULL))
        return 0;

    if (G_UNLIKELY(a == NULL))
        return -1;

    if (G_UNLIKELY(b == NULL))
        return 1;

    diff = difftime(ts2->start, ts1->start);
    if (diff > 0)
        return -1;
    if (diff < 0)
        return 1;

    /* start time is equal, now it's easy to check end time ;-) */
    return (gint) difftime(ts2->end, ts1->end);
}


static void
point_data_results_free(point_data_results *pdr)
{
    g_assert(pdr != NULL);
    if (G_UNLIKELY(pdr == NULL))
        return;

    g_assert(pdr->before != NULL);
    g_array_free(pdr->before, FALSE);
    g_assert(pdr->after != NULL);
    g_array_free(pdr->after, FALSE);
    g_slice_free(point_data_results, pdr);
    return;
}

/*
 * Given an array of point data, find two points for which
 * corresponding interval data can be found so that the interval is as
 * small as possible, returning NULL if such interval data doesn't
 * exist.
 */
static xml_time *
find_smallest_interval(xml_weather *wd,
                       const point_data_results *pdr)
{
    GArray *before = pdr->before, *after = pdr->after;
    xml_time *ts_before, *ts_after, *found;
    gint i, j;

    for (i = before->len - 1; i >= 0; i--) {
        ts_before = g_array_index(before, xml_time *, i);
        for (j = 0; j < after->len; j++) {
            ts_after = g_array_index(after, xml_time *, j);
            found = get_timeslice(wd, ts_before->start, ts_after->end, NULL);
            if (found)
                return found;
        }
    }
    return NULL;
}


/*
 * Given an array of point data, find two points for which
 * corresponding interval data can be found so that the interval is as
 * big as possible, returning NULL if such interval data doesn't
 * exist.
 */
static xml_time *
find_largest_interval(xml_weather *wd,
                      const point_data_results *pdr)
{
    GArray *before = pdr->before, *after = pdr->after;
    xml_time *ts_before = NULL, *ts_after = NULL, *found = NULL;
    gint i, j;

    for (i = before->len - 1; i >= 0; i--) {
        ts_before = g_array_index(before, xml_time *, i);
        g_assert(ts_before != NULL);
        for (j = after->len - 1; j >= 0; j--) {
            ts_after = g_array_index(after, xml_time *, j);
            found = get_timeslice(wd, ts_before->start, ts_after->end, NULL);
            if (found) {
                weather_debug("Found biggest interval:");
                weather_dump(weather_dump_timeslice, found);
                return found;
            }
        }
    }
    weather_debug("Could not find interval.");
    return NULL;
}


/* find point data within certain limits around a point in time */
static point_data_results *
find_point_data(const xml_weather *wd,
                const time_t point_t,
                const gdouble min_diff,
                const gdouble max_diff)
{
    point_data_results *found;
    xml_time *timeslice;
    guint i;
    gdouble diff;

    found = g_slice_new0(point_data_results);
    found->before = g_array_new(FALSE, TRUE, sizeof(xml_time *));
    found->after = g_array_new(FALSE, TRUE, sizeof(xml_time *));

    weather_debug("Checking %d timeslices for point data.",
                  wd->timeslices->len);
    for (i = 0; i < wd->timeslices->len; i++) {
        timeslice = g_array_index(wd->timeslices, xml_time *, i);
        /* look only for point data, not intervals */
        if (G_UNLIKELY(timeslice == NULL) || timeslice_is_interval(timeslice))
            continue;

        /* add point data if within limits */
        diff = difftime(timeslice->end, point_t);
        if (diff < 0) {   /* before point_t */
            diff *= -1;
            if (diff < min_diff || diff > max_diff)
                continue;
            g_array_append_val(found->before, timeslice);
            weather_dump(weather_dump_timeslice, timeslice);
        } else {          /* after point_t */
            if (diff < min_diff || diff > max_diff)
                continue;
            g_array_append_val(found->after, timeslice);
            weather_dump(weather_dump_timeslice, timeslice);
        }
    }
    g_array_sort(found->before, (GCompareFunc) xml_time_compare);
    g_array_sort(found->after, (GCompareFunc) xml_time_compare);
    found->point = point_t;
    weather_debug("Found %d timeslices with point data, "
                  "%d before and %d after point_t.",
                  (found->before->len + found->after->len),
                  found->before->len, found->after->len);
    return found;
}


xml_time *
make_current_conditions(xml_weather *wd,
                        time_t now_t)
{
    point_data_results *found = NULL;
    xml_time *interval = NULL;
    struct tm point_tm = *localtime(&now_t);
    time_t point_t = now_t;
    gint i = 0;

    g_assert(wd != NULL);
    if (G_UNLIKELY(wd == NULL))
        return NULL;

    /* there may not be a timeslice available for the current
       interval, so look max three hours ahead */
    while (i < 3 && interval == NULL) {
        point_t = time_calc_hour(point_tm, i);
        found = find_point_data(wd, point_t, 1, 4 * 3600);
        interval = find_smallest_interval(wd, found);
        point_data_results_free(found);
        point_tm = *localtime(&point_t);
        i++;
    }
    weather_dump(weather_dump_timeslice, interval);
    if (interval == NULL)
        return NULL;

    return make_combined_timeslice(wd, interval, &now_t);
}


static time_t
get_daytime(int day,
            daytime dt)
{
    struct tm point_tm;
    time_t point_t;

    /* initialize times to the current day */
    time(&point_t);
    point_tm = *localtime(&point_t);

    /* calculate daytime for the requested day */
    point_tm.tm_mday += day;
    point_tm.tm_min = point_tm.tm_sec = 0;
    point_tm.tm_isdst = -1;
    switch (dt) {
    case MORNING:
        point_tm.tm_hour = 9;
        break;
    case AFTERNOON:
        point_tm.tm_hour = 15;
        break;
    case EVENING:
        point_tm.tm_hour = 21;
        break;
    case NIGHT:
        point_tm.tm_hour = 27;
        break;
    }
    point_t = mktime(&point_tm);
    return point_t;
}


/*
 * Get forecast data for a given daytime for the day (today + day).
 */
xml_time *
make_forecast_data(xml_weather *wd,
                   int day,
                   daytime dt)
{
    point_data_results *found = NULL;
    xml_time *interval = NULL;
    time_t point_t;

    g_assert(wd != NULL);
    if (G_UNLIKELY(wd == NULL))
        return NULL;

    point_t = get_daytime(day, dt);
    found = find_point_data(wd, point_t, 1, 5 * 3600);
    interval = find_largest_interval(wd, found);
    point_data_results_free(found);
    weather_dump(weather_dump_timeslice, interval);
    if (interval == NULL)
        return NULL;

    return make_combined_timeslice(wd, interval, &point_t);
}
