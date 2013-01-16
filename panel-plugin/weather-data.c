/*  Copyright (c) 2003-2013 Xfce Development Team
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
#include <math.h>

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
                 const gdouble backup)
{
    gdouble d = backup;
    if (str && strlen(str) > 0)
        d = g_ascii_strtod(str, NULL);
    return d;
}


/* convert double to string, using non-local format */
gchar *
double_to_string(const gdouble val,
                 const gchar *format)
{
    gchar buf[20];
    return g_strdup(g_ascii_formatd(buf, 20,
                                    format ? format : "%.1f",
                                    val));
}


gchar *
format_date(const time_t date_t,
            gchar *format,
            gboolean local)
{
    struct tm *tm;
    time_t t = date_t;
    gchar buf[40];
    size_t size;

    if (G_LIKELY(local))
        tm = localtime(&t);
    else
        tm = gmtime(&t);

    /* A year <= 1970 means date has not been set */
    if (G_UNLIKELY(tm == NULL) || tm->tm_year <= 70)
        return g_strdup("-");
    if (format == NULL)
        format = "%Y-%m-%d %H:%M:%S";
    size = strftime(buf, 40, format, tm);
    return (size ? g_strdup(buf) : g_strdup("-"));
}


/* check whether timeslice is interval or point data */
gboolean
timeslice_is_interval(xml_time *timeslice)
{
    return (timeslice->location->symbol != NULL ||
            timeslice->location->precipitation_value != NULL);
}


/*
 * Calculate dew point in Celsius, taking the Magnus formulae as a
 * basis. Source: http://de.wikipedia.org/wiki/Taupunkt
 */
static gdouble
calc_dewpoint(const xml_location *loc)
{
    gdouble temp = string_to_double(loc->temperature_value, 0);
    gdouble humidity = string_to_double(loc->humidity_value, 0);
    gdouble val = log(humidity / 100);

    return (241.2 * val + 4222.03716 * temp / (241.2 + temp))
        / (17.5043 - val - 17.5043 * temp / (241.2 + temp));
}


/* Calculate felt air temperature, using the chosen model. */
static gdouble
calc_apparent_temperature(const xml_location *loc,
                          const apparent_temp_models model,
                          const gboolean night_time)
{
    gdouble temp = string_to_double(loc->temperature_value, 0);
    gdouble windspeed = string_to_double(loc->wind_speed_mps, 0);
    gdouble humidity = string_to_double(loc->humidity_value, 0);
    gdouble dp, e;

    switch (model) {
    case WINDCHILL_HEATINDEX:
        /* If temperature is lower than 10 °C, use wind chill index,
           if above 26.7°C use the heat index / Summer Simmer Index. */

        /* Wind chill, source:
           http://www.nws.noaa.gov/os/windchill/index.shtml */
        if (temp <= 10.0) {
            /* wind chill is only defined for wind speeds above 3.0 mph */
            windspeed *= 3.6;
            if (windspeed < 4.828032)
                return temp;

            return 13.12 + 0.6215 * temp - 11.37 * pow(windspeed, 0.16)
                + 0.3965 * temp * pow(windspeed, 0.16);
         }

        if (temp >= 26.7 || (night_time && temp >= 22.0)) {
            /* humidity needs to be higher than 40% for a valid result */
            if (humidity < 40)
                return temp;

            temp = temp * 9.0 / 5.0 + 32.0;  /* both models use Fahrenheit */
            if (!night_time)
                /* Heat index, source:
                   Lans P. Rothfusz. "The Heat Index 'Equation' (or, More
                   Than You Ever Wanted to Know About Heat Index)",
                   Scientific Services Division (NWS Southern Region
                   Headquarters), 1 July 1990.
                   http://www.srh.noaa.gov/images/ffc/pdf/ta_htindx.PDF
                */
                return ((-42.379
                         + 2.04901523 * temp
                         + 10.14333127 * humidity
                         - 0.22475541 * temp * humidity
                         - 0.00683783 * temp * temp
                         - 0.05481717 * humidity * humidity
                         + 0.00122874 * temp * temp * humidity
                         + 0.00085282 * temp * humidity * humidity
                         - 0.00000199 * temp * temp * humidity * humidity)
                        - 32.0) * 5.0 / 9.0;   /* convert back to Celsius */
            else
                /* Summer Simmer Index, sources:
                   http://www.summersimmer.com/home.htm
                   http://www.gorhamschaffler.com/humidity_formulas.htm */
                return ((1.98 * (temp - (0.55 - 0.0055 * humidity)
                                 * (temp - 58)) - 56.83)
                        - 32.0) * 5.0 / 9.0;   /* convert back to Celsius */
        }

        /* otherwise simply return the temperature */
        return temp;

    case WINDCHILL_HUMIDEX:
        /* If temperature is equal or lower than 0 °C, use wind chill index,
           if above 20.0 °C use humidex. Source:
           http://www.weatheroffice.gc.ca/mainmenu/faq_e.html */

        if (temp <= 0) {
            /* wind chill is only defined for wind speeds above 2.0 km/h */
            windspeed *= 3.6;
            if (windspeed < 2.0)
                return temp;

            /* wind chill, source:
               http://www.nws.noaa.gov/os/windchill/index.shtml */
            return 13.12 + 0.6215 * temp - 11.37 * pow(windspeed, 0.16)
                + 0.3965 * temp * pow(windspeed, 0.16);
        }

        if (temp >= 20.0) {
            /* Canadian humidex, source:
               http://www.weatheroffice.gc.ca/mainmenu/faq_e.html#weather6 */
            dp = calc_dewpoint(loc);

            /* dew point needs to be above a certain limit for
               valid results, see
               http://www.weatheroffice.gc.ca/mainmenu/faq_e.html#weather5 */
            if (dp < 0)
                return temp;

            /* dew point needs to be converted to Kelvin (easy job ;-) */
            e = 6.11 * exp(5417.7530 * (1/273.16 - 1/(dp + 273.15)));
            return temp + 0.5555 * (e - 10.0);
        }
        return temp;

    case STEADMAN:
        /* Australians use a different formula. Source:
           http://www.bom.gov.au/info/thermal_stress/#atapproximation */
        e = humidity / 100 * 6.105 * exp(17.27 * temp / (237.7 + temp));
        return temp + 0.33 * e - 0.7 * windspeed - 4.0;

    case QUAYLE_STEADMAN:
        /* R. G. Quayle, R. G. Steadman: The Steadman wind chill: an
           improvement over present scales. In: Weather and
           Forecasting. 13, 1998, S. 1187–1193 */
        return 1.41 - 1.162 * windspeed + 0.980 * temp
            + 0.0124 * windspeed * windspeed + 0.0185 * windspeed * temp;
    }
}


gchar *
get_data(const xml_time *timeslice,
         const units_config *units,
         const data_types type,
         const gboolean round,
         const gboolean night_time)
{
    const xml_location *loc = NULL;
    gdouble val, temp;

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

    case TEMPERATURE:      /* source is in °C */
        val = string_to_double(loc->temperature_value, 0);
        if (units->temperature == FAHRENHEIT)
            val = val * 9.0 / 5.0 + 32.0;
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
        case FTS:
            val *= 3.2808399;
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

    case DEWPOINT:
        val = calc_dewpoint(loc);
        if (units->temperature == FAHRENHEIT)
            val = val * 9.0 / 5.0 + 32.0;
        return g_strdup_printf(ROUND_TO_INT("%.1f"), val);

    case APPARENT_TEMPERATURE:
        val = calc_apparent_temperature(loc, units->apparent_temperature,
                                        night_time);
        if (units->temperature == FAHRENHEIT)
            val = val * 9.0 / 5.0 + 32.0;
        return g_strdup_printf(ROUND_TO_INT("%.1f"), val);

    case CLOUDS_LOW:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_LOW],
                             ROUND_TO_INT("%.1f"));

    case CLOUDS_MID:
        return LOCALE_DOUBLE(loc->clouds_percent[CLOUDS_PERC_MID],
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

        /* For snow, adjust precipitations dependent on temperature. Source:
           http://answers.yahoo.com/question/index?qid=20061230123635AAAdZAe */
        if (loc->symbol_id == SYMBOL_SNOWSUN ||
            loc->symbol_id == SYMBOL_SNOW ||
            loc->symbol_id == SYMBOL_SNOWTHUNDER ||
            loc->symbol_id == SYMBOL_SNOWSUNPOLAR ||
            loc->symbol_id == SYMBOL_SNOWSUNTHUNDER) {
            temp = string_to_double(loc->temperature_value, 0);
            if (temp < -11.1111)      /* below 12 °F, low snow density */
                val *= 12;
            else if (temp < -4.4444)  /* 12 to 24 °F, still low density */
                val *= 10;
            else if (temp < -2.2222)  /* 24 to 28 °F, more density */
                val *= 7;
            else if (temp < -0.5556)  /* 28 to 31 °F, wet, dense, melting */
                val *= 5;
            else                      /* anything above 31 °F */
                val *= 3;
        }

        if (units->precipitations == INCHES) {
            val /= 25.4;
            return g_strdup_printf("%.2f", val);
        } else
            return g_strdup_printf("%.1f", val);

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
    case DEWPOINT:
    case APPARENT_TEMPERATURE:
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
        case FTS:
            return _("ft/s");
        case KNOTS:
            return _("kt");
        }
    case WIND_DIRECTION_DEG:
    case LATITUDE:
    case LONGITUDE:
        /* TRANSLATORS: The degree sign is used like a unit for
           latitude, longitude, wind direction */
        return _("°");
    case HUMIDITY:
    case CLOUDS_LOW:
    case CLOUDS_MID:
    case CLOUDS_HIGH:
    case CLOUDINESS:
    case FOG:
        /* TRANSLATORS: Percentage sign is used like a unit for
           clouds, fog, humidity */
        return _("%");
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


static void
calculate_symbol(xml_time *timeslice)
{
    xml_location *loc;
    gdouble fog, cloudiness, precipitation;

    g_assert(timeslice != NULL && timeslice->location != NULL);
    if (G_UNLIKELY(timeslice == NULL || timeslice->location == NULL))
        return;

    loc = timeslice->location;

    precipitation = string_to_double(loc->precipitation_value, 0);
    if (precipitation > 0)
        return;

    cloudiness =
        string_to_double(loc->clouds_percent[CLOUDS_PERC_CLOUDINESS], 0);
    if (cloudiness >= 90)
        loc->symbol_id = SYMBOL_CLOUD;
    else if (cloudiness >= 30)
        loc->symbol_id = SYMBOL_PARTLYCLOUD;
    else if (cloudiness >= 1.0 / 8.0)
        loc->symbol_id = SYMBOL_LIGHTCLOUD;

    fog = string_to_double(loc->fog_percent, 0);
    if (fog >= 80)
        loc->symbol_id = SYMBOL_FOG;

    /* update symbol name */
    g_free(loc->symbol);
    loc->symbol = g_strdup(symbol_names[loc->symbol_id]);
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
    return double_to_string(val_result, "%.1f");
}


/* Create a new combined timeslice, with optionally interpolated data */
static xml_time *
make_combined_timeslice(xml_weather *wd,
                        const xml_time *interval,
                        const time_t *between_t)
{
    xml_time *comb, *start, *end;
    gboolean ipol = (between_t != NULL) ? TRUE : FALSE;
    gint i;

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

    calculate_symbol(comb);
    return comb;
}


void
merge_timeslice(xml_weather *wd,
                const xml_time *timeslice)
{
    xml_time *old_ts, *new_ts;
    time_t now_t = time(NULL);
    guint index;

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
    old_ts = get_timeslice(wd, timeslice->start, timeslice->end, &index);
    if (old_ts) {
        xml_time_free(old_ts);
        g_array_remove_index(wd->timeslices, index);
        g_array_insert_val(wd->timeslices, index, new_ts);
        weather_debug("Replaced existing timeslice at %d.", index);
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
 * Compare two xml_time structs using their start and end times,
 * returning the result as a qsort()-style comparison function (less
 * than zero for first arg is less than second arg, zero for equal,
 * greater zero if first arg is greater than second arg).
 */
gint
xml_time_compare(gconstpointer a,
                 gconstpointer b)
{
    xml_time *ts1 = *(xml_time **) a;
    xml_time *ts2 = *(xml_time **) b;
    gdouble diff;

    if (G_UNLIKELY(ts1 == NULL && ts2 == NULL))
        return 0;
    if (G_UNLIKELY(ts1 == NULL))
        return -1;
    if (G_UNLIKELY(ts2 == NULL))
        return 1;

    diff = difftime(ts2->start, ts1->start);
    if (diff != 0)
        return (gint) (diff * -1);

    /* start time is equal, now it's easy to check end time ;-) */
    return (gint) (difftime(ts2->end, ts1->end) * -1);
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


static xml_time *
find_smallest_incomplete_interval(xml_weather *wd,
                                  time_t end_t)
{
    xml_time *timeslice, *found = NULL;
    gint i;

    weather_debug("Searching for the smallest incomplete interval.");
    /* search for all timeslices with interval data that have end time end_t */
    for (i = 0; i < wd->timeslices->len; i++) {
        timeslice = g_array_index(wd->timeslices, xml_time *, i);
        if (timeslice && difftime(timeslice->end, end_t) == 0
            && difftime(timeslice->end, timeslice->start) != 0) {
            if (found == NULL)
                found = timeslice;
            else
                if (difftime(timeslice->start, found->start) > 0)
                    found = timeslice;
            weather_dump(weather_dump_timeslice, found);
        }
    }
    weather_debug("Search result for smallest incomplete interval is:");
    weather_dump(weather_dump_timeslice, found);
    return found;
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
    gdouble diff;
    gint i;

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
    xml_time *interval = NULL, *incomplete;
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

        /* There may be interval data where point data is only
           available at the end of that interval. If such an interval
           exists, use it, it's still better than the next one where
           now_t is not in between. */
        if (interval && difftime(interval->start, now_t) > 0)
            if ((incomplete =
                 find_smallest_incomplete_interval(wd, interval->start)))
                interval = incomplete;
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
