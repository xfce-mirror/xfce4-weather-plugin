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

/*
 * The following two defines fix compile warnings and need to be
 * before time.h and libxfce4panel.h (which includes glib.h).
 * Otherwise, they will be ignored.
 */
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1
#include "weather-parsers.h"
#include "weather-translate.h"
#include "weather-debug.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>


#define DATA(node)                                                  \
    ((gchar *) xmlNodeListGetString(node->doc, node->children, 1))

#define PROP(node, prop)                                        \
    ((gchar *) xmlGetProp((node), (const xmlChar *) (prop)))

#define NODE_IS_TYPE(node, type)                        \
    (xmlStrEqual(node->name, (const xmlChar *) type))


/*
 * This is a portable replacement for the deprecated timegm(),
 * copied from the man page and modified to use GLIB functions.
 */
static time_t
my_timegm(struct tm *tm)
{
    time_t ret;
    GDateTime *initial_date;

    initial_date = g_date_time_new_utc(tm->tm_year + 1900,
                                       tm->tm_mon + 1,
                                       tm->tm_mday,
                                       tm->tm_hour,
                                       tm->tm_min,
                                       tm->tm_sec);
    ret = g_date_time_to_unix(initial_date);
    g_date_time_unref(initial_date);

    return ret;
}


/*
 * Remove offset of timezone, in order to keep previous
 * date format (before the new API, 2.x).
 */
static gchar *
remove_timezone_offset(const gchar *date)
{
    GRegex *re = NULL;
    const gchar *pattern = "[+-][0-9]{2}:[0-9]{2}";
    gchar *res;

    re = g_regex_new(pattern, 0, 0, NULL);
    if (re != NULL && g_regex_match(re, date, 0, NULL)) {
        res = g_regex_replace(re, date, -1, 0, "Z", 0, NULL);
    } else {
        res = g_strdup(date);
    }
    g_regex_unref(re);
    return res;
}


xml_time *
get_timeslice(xml_weather *wd,
              const time_t start_t,
              const time_t end_t,
              guint *index)
{
    xml_time *timeslice;
    guint i;

    g_assert(wd != NULL);
    if (G_UNLIKELY(wd == NULL))
        return NULL;

    for (i = 0; i < wd->timeslices->len; i++) {
        timeslice = g_array_index(wd->timeslices, xml_time *, i);
        if (timeslice &&
            timeslice->start == start_t && timeslice->end == end_t) {
            if (index != NULL)
                *index = i;
            return timeslice;
        }
    }
    return NULL;
}


xml_astro *
get_astro(const GArray *astrodata,
          const time_t day_t,
          guint *index)
{
    xml_astro *astro;
    guint i;

    g_assert(astrodata != NULL);
    if (G_UNLIKELY(astrodata == NULL))
        return NULL;

    weather_debug("day_t=%s", format_date(day_t, NULL,TRUE));
    for (i = 0; i < astrodata->len; i++) {
        astro = g_array_index(astrodata, xml_astro *, i);
        weather_debug("astro->day=%s", format_date(astro->day, NULL,TRUE));
        if (astro && astro->day == day_t) {
            if (index != NULL)
                *index = i;
            return astro;
        }
    }
    return NULL;
}


time_t
parse_timestring(const gchar *ts,
                 const gchar *format,
                 gboolean local) {
    time_t t;
    struct tm tm;

    if (G_UNLIKELY(ts == NULL)) {
        memset(&t, 0, sizeof(time_t));
        return t;
    }

    /* standard format */
    if (format == NULL)
        format = "%Y-%m-%dT%H:%M:%SZ";

    /* strptime needs an initialized struct, or unpredictable
     * behaviour might occur */
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_isdst = -1;

    if (strptime(ts, format, &tm) == NULL) {
        memset(&t, 0, sizeof(time_t));
        return t;
    }

    t = local ? mktime(&tm) : my_timegm(&tm);

    if (t < 0)
        memset(&t, 0, sizeof(time_t));

    return t;
}


static const gchar *
parse_moonposition (gdouble pos_in) {
    gdouble pos = pos_in / 360.0 * 100.0;
    if (pos < 0.0 || pos > 100.0)
        return "Unknown";
    if (pos <= 12.5)
        return "Waxing crescent";
    else if (pos <= 25.0)
        return "First quarter";
    else if (pos <= 37.5)
        return "Waxing gibbous";
    else if (pos <= 50.0)
        return "Full moon";
    else if (pos <= 62.5)
        return "Waning gibbous";
    else if (pos <= 75.0)
        return "Third quarter";
    else if (pos <= 87.5)
        return "Waning crescent";
    return "New moon";
}


static void
parse_location(xmlNode *cur_node,
               xml_location *loc)
{
    xmlNode *child_node;

    g_free(loc->altitude);
    loc->altitude = PROP(cur_node, "altitude");

    g_free(loc->latitude);
    loc->latitude = PROP(cur_node, "latitude");

    g_free(loc->longitude);
    loc->longitude = PROP(cur_node, "longitude");

    for (child_node = cur_node->children; child_node;
         child_node = child_node->next) {
        if (NODE_IS_TYPE(child_node, "temperature")) {
            g_free(loc->temperature_unit);
            g_free(loc->temperature_value);
            loc->temperature_unit = PROP(child_node, "unit");
            loc->temperature_value = PROP(child_node, "value");
        }
        if (NODE_IS_TYPE(child_node, "windDirection")) {
            g_free(loc->wind_dir_deg);
            g_free(loc->wind_dir_name);
            loc->wind_dir_deg = PROP(child_node, "deg");
            loc->wind_dir_name = PROP(child_node, "name");
        }
        if (NODE_IS_TYPE(child_node, "windSpeed")) {
            g_free(loc->wind_speed_mps);
            g_free(loc->wind_speed_beaufort);
            loc->wind_speed_mps = PROP(child_node, "mps");
            loc->wind_speed_beaufort = PROP(child_node, "beaufort");
        }
        if (NODE_IS_TYPE(child_node, "humidity")) {
            g_free(loc->humidity_unit);
            g_free(loc->humidity_value);
            loc->humidity_unit = PROP(child_node, "unit");
            loc->humidity_value = PROP(child_node, "value");
        }
        if (NODE_IS_TYPE(child_node, "pressure")) {
            g_free(loc->pressure_unit);
            g_free(loc->pressure_value);
            loc->pressure_unit = PROP(child_node, "unit");
            loc->pressure_value = PROP(child_node, "value");
        }
        if (NODE_IS_TYPE(child_node, "cloudiness")) {
            g_free(loc->clouds_percent[CLOUDS_PERC_CLOUDINESS]);
            loc->clouds_percent[CLOUDS_PERC_CLOUDINESS] = PROP(child_node, "percent");
        }
        if (NODE_IS_TYPE(child_node, "fog")) {
            g_free(loc->fog_percent);
            loc->fog_percent = PROP(child_node, "percent");
        }
        if (NODE_IS_TYPE(child_node, "lowClouds")) {
            g_free(loc->clouds_percent[CLOUDS_PERC_LOW]);
            loc->clouds_percent[CLOUDS_PERC_LOW] = PROP(child_node, "percent");
        }
        if (NODE_IS_TYPE(child_node, "mediumClouds")) {
            g_free(loc->clouds_percent[CLOUDS_PERC_MID]);
            loc->clouds_percent[CLOUDS_PERC_MID] = PROP(child_node, "percent");
        }
        if (NODE_IS_TYPE(child_node, "highClouds")) {
            g_free(loc->clouds_percent[CLOUDS_PERC_HIGH]);
            loc->clouds_percent[CLOUDS_PERC_HIGH] = PROP(child_node, "percent");
        }
        if (NODE_IS_TYPE(child_node, "precipitation")) {
            g_free(loc->precipitation_unit);
            g_free(loc->precipitation_value);
            loc->precipitation_unit = PROP(child_node, "unit");
            loc->precipitation_value = PROP(child_node, "value");
        }
        if (NODE_IS_TYPE(child_node, "symbol")) {
            g_free(loc->symbol);
            loc->symbol_id = strtol(PROP(child_node, "number"), NULL, 10);
            loc->symbol = g_strdup(get_symbol_for_id(loc->symbol_id));
        }
    }

    /* Convert Fahrenheit to Celsius if necessary, so that we don't
       have to do it later. met.no usually provides values in Celsius. */
    if (loc->temperature_value && loc->temperature_unit &&
        !strcmp(loc->temperature_unit, "fahrenheit")) {
        gdouble val = string_to_double(loc->temperature_value, 0);
        val = (val - 32.0) * 5.0 / 9.0;
        g_free(loc->temperature_value);
        loc->temperature_value = double_to_string(val, "%.1f");
        g_free(loc->temperature_unit);
        loc->temperature_unit = g_strdup("celsius");
    }
}


xml_weather *
make_weather_data(void)
{
    xml_weather *wd;

    wd = g_slice_new0(xml_weather);
    if (G_UNLIKELY(wd == NULL))
        return NULL;
    wd->timeslices = g_array_sized_new(FALSE, TRUE,
                                       sizeof(xml_time *), 200);
    if (G_UNLIKELY(wd->timeslices == NULL)) {
        g_slice_free(xml_weather, wd);
        return NULL;
    }
    return wd;
}


xml_time *
make_timeslice(void)
{
    xml_time *timeslice;

    timeslice = g_slice_new0(xml_time);
    if (G_UNLIKELY(timeslice == NULL))
        return NULL;

    timeslice->location = g_slice_new0(xml_location);
    if (G_UNLIKELY(timeslice->location == NULL)) {
        g_slice_free(xml_time, timeslice);
        return NULL;
    }
    return timeslice;
}


static void
parse_time(xmlNode *cur_node,
           xml_weather *wd)
{
    gchar *datatype, *from, *to;
    time_t start_t, end_t;
    xml_time *timeslice;
    xmlNode *child_node;

    datatype = PROP(cur_node, "datatype");
    if (xmlStrcasecmp((xmlChar *) datatype, (xmlChar *) "forecast")) {
        xmlFree(datatype);
        return;
    }
    xmlFree(datatype);

    from = PROP(cur_node, "from");
    start_t = parse_timestring(from, NULL, FALSE);
    xmlFree(from);

    to = PROP(cur_node, "to");
    end_t = parse_timestring(to, NULL, FALSE);
    xmlFree(to);

    if (G_UNLIKELY(!start_t || !end_t))
        return;

    /* look for existing timeslice or add a new one */
    timeslice = get_timeslice(wd, start_t, end_t, NULL);
    if (! timeslice) {
        timeslice = make_timeslice();
        if (G_UNLIKELY(!timeslice))
            return;
        timeslice->start = start_t;
        timeslice->end = end_t;
        g_array_append_val(wd->timeslices, timeslice);
    }

    for (child_node = cur_node->children; child_node;
         child_node = child_node->next)
        if (G_LIKELY(NODE_IS_TYPE(child_node, "location")))
            parse_location(child_node, timeslice->location);
}


/*
 * Parse XML weather data and merge it with current data.
 */
gboolean
parse_weather(xmlNode *cur_node,
              xml_weather *wd)
{
    xmlNode *child_node;

    g_assert(wd != NULL);
    if (G_UNLIKELY(wd == NULL))
        return FALSE;

    if (G_UNLIKELY(cur_node == NULL || !NODE_IS_TYPE(cur_node, "weatherdata")))
        return FALSE;

    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE(cur_node, "product")) {
            gchar *class = PROP(cur_node, "class");
            if (xmlStrcasecmp((xmlChar *) class, (xmlChar *) "pointData")) {
                xmlFree(class);
                continue;
            }
            g_free(class);
            for (child_node = cur_node->children; child_node;
                 child_node = child_node->next)
                if (NODE_IS_TYPE(child_node, "time"))
                    parse_time(child_node, wd);
        }
    }
    return TRUE;
}


/*
 * Look at https://docs.api.met.no/doc/formats/SunriseJSON for information
 * of elements and attributes to expect.
 */
gboolean
parse_astrodata_sun(json_object *cur_node,
                    GArray *astrodata)
{
    xml_astro *astro;
    json_object *jwhen, *jinterval, *jproperties, *jdate, *jsunrise,
                *jsunrise_time, *jsunset, *jsunset_time,
                *jsolarnoon, *jsolarmidnight, *jdisc_centre_elevation;
    const gchar day_format[]="%Y-%m-%dT%H:%M:%SZ";
    const gchar sun_format[]="%Y-%m-%dT%H:%MZ";
    const gchar *date;
    gchar *time;
    gboolean sun_rises = FALSE, sun_sets = FALSE;

    astro = g_slice_new0(xml_astro);
    if (G_UNLIKELY(astro == NULL))
        return FALSE;

    g_assert(astrodata != NULL);
    if (G_UNLIKELY(astrodata == NULL))
        return FALSE;

    jwhen = json_object_object_get(cur_node, "when");
    if (G_UNLIKELY(jwhen == NULL))
        return FALSE;

    jinterval = json_object_object_get(jwhen, "interval");
    if (G_UNLIKELY(jinterval == NULL))
        return FALSE;
    if (G_UNLIKELY(json_object_array_length(jinterval)!=2))
        return FALSE;

    jdate = json_object_array_get_idx(jinterval, 0);
    if (G_UNLIKELY(jdate == NULL))
        return FALSE;

    date = json_object_get_string(jdate);
    if (G_UNLIKELY(date == NULL))
        return FALSE;

    /* use time info at center of day interval */
    astro->day = day_at_midnight(parse_timestring(date, day_format, FALSE) + 12 * 3600, 0);
    weather_debug("sun: astro->day=%s\n",
                  format_date(astro->day, day_format,TRUE)); 

    jproperties = json_object_object_get(cur_node, "properties");
    if (G_UNLIKELY(jproperties == NULL))
        return FALSE;

    jsunrise = json_object_object_get(jproperties, "sunrise");
    if (G_UNLIKELY(jsunrise == NULL))
        return FALSE;

    jsunrise_time = json_object_object_get(jsunrise, "time");
    if (G_UNLIKELY(jsunrise_time == NULL)) {
        weather_debug("sunrise time not found");
    } else {
        date = json_object_get_string(jsunrise_time);
        if (G_UNLIKELY(date == NULL))
            return FALSE;
        time = remove_timezone_offset(date);
        astro->sunrise= parse_timestring(time, sun_format, TRUE);
        sun_rises = TRUE;
        weather_debug("astro->sunrise=%s\n",
                      format_date(astro->sunrise, NULL,TRUE));
        g_free(time);
    }

    jsunset = json_object_object_get(jproperties, "sunset");
    if (G_UNLIKELY(jsunset == NULL))
        return FALSE;

    jsunset_time = json_object_object_get(jsunset, "time");
    if (G_UNLIKELY(jsunset_time == NULL)) {
        weather_debug("sunset time not found");
    } else {
        date = json_object_get_string(jsunset_time);
        if (G_UNLIKELY(date == NULL))
            return FALSE;
        time = remove_timezone_offset(date);
        astro->sunset= parse_timestring(time, sun_format, TRUE);
        sun_sets = TRUE;
        weather_debug("astro->sunset=%s\n",
                      format_date(astro->sunset, NULL,TRUE));
        g_free(time);
    }

    jsolarnoon = json_object_object_get(jproperties, "solarnoon");
    if (G_UNLIKELY(jsolarnoon == NULL))
        return FALSE;

    jdisc_centre_elevation = json_object_object_get(jsolarnoon, "disc_centre_elevation");
    if (G_UNLIKELY(jdisc_centre_elevation == NULL))
        return FALSE;

    astro->solarnoon_elevation = json_object_get_double(jdisc_centre_elevation);
    weather_debug("astro->solarnoon_elevation=%f\n",
                  astro->solarnoon_elevation);

    jsolarmidnight = json_object_object_get(jproperties, "solarmidnight");
    if (G_UNLIKELY(jsolarmidnight == NULL))
        return FALSE;

    jdisc_centre_elevation = json_object_object_get(jsolarmidnight, "disc_centre_elevation");
    if (G_UNLIKELY(jdisc_centre_elevation == NULL))
        return FALSE;

    astro->solarmidnight_elevation = json_object_get_double(jdisc_centre_elevation);
    weather_debug("astro->solarmidnight_elevation=%f\n",
                  astro->solarmidnight_elevation);

    if (sun_rises)
        astro->sun_never_rises = FALSE;
    else
        astro->sun_never_rises = TRUE;

    if (sun_sets)
        astro->sun_never_sets = FALSE;
    else
        astro->sun_never_sets = TRUE;

    merge_astro(astrodata, astro);
    xml_astro_free(astro);
    return TRUE;
}


/*
 * Look at https://docs.api.met.no/doc/formats/SunriseJSON for information
 * of elements and attributes to expect.
 */
gboolean
parse_astrodata_moon(json_object *cur_node,
                     GArray *astrodata)
{
    xml_astro *astro;
    json_object *jwhen, *jinterval, *jproperties, *jdate, *jmoonrise,
                *jmoonrise_time, *jmoonset,  *jmoonset_time, *jmoonphase;
    time_t day;
    guint index;
    const gchar day_format[]="%Y-%m-%dT%H:%M:%SZ";
    const gchar moon_format[]="%Y-%m-%dT%H:%MZ";
    const gchar *date;
    gchar *time;
    gboolean moon_rises = FALSE, moon_sets = FALSE;

    g_assert(astrodata != NULL);
    if (G_UNLIKELY(astrodata == NULL))
        return FALSE;

    jwhen = json_object_object_get(cur_node, "when");
    if (G_UNLIKELY(jwhen == NULL)) {
        weather_debug("when not found" );
        return FALSE;
    }

    jinterval = json_object_object_get(jwhen, "interval");
    if (G_UNLIKELY(jinterval == NULL)) {
        weather_debug("interval not found" );
        return FALSE;
    }
    if (G_UNLIKELY(json_object_array_length(jinterval)!=2)) {
        weather_debug("interval length is %d instead of %d",
                      json_object_array_length(jinterval));
        return FALSE;
    }

    jdate = json_object_array_get_idx(jinterval, 0);
    if (G_UNLIKELY(jdate == NULL)) {
        weather_debug("jdate empty" );
        return FALSE;
    }

    date = json_object_get_string(jdate);
    if (G_UNLIKELY(date == NULL)) {
        weather_debug("date not found" );
        return FALSE;
    }

    /* use time info at center of day interval */
    day = day_at_midnight(parse_timestring(date, day_format, FALSE) + 12 * 3600, 0);
    /* this data seems weird */
    astro = get_astro(astrodata, day, &index);
    if (G_UNLIKELY(astro == NULL)) {
        weather_debug("no sun astrodata for day=%s\n",
                      format_date(day, day_format,FALSE));
        return FALSE;
    }

    astro->day=day;
    weather_debug("moon: astro->day=%s\n", format_date(astro->day, day_format,TRUE));

    jproperties = json_object_object_get(cur_node, "properties");
    if (G_UNLIKELY(jproperties == NULL)) {
        weather_debug("properties not found" );
        return FALSE;
    }

    jmoonrise = json_object_object_get(jproperties, "moonrise");
    if (G_UNLIKELY(jmoonrise == NULL)) {
        weather_debug("moonrise not found" );
        return FALSE;
    }

    jmoonrise_time = json_object_object_get(jmoonrise, "time");
    if (G_UNLIKELY(jmoonrise_time == NULL)) {
        weather_debug("moonrise time not found" );
    } else {
        date = json_object_get_string(jmoonrise_time);
        if (G_UNLIKELY(date == NULL)) {
            weather_debug("jmoonrise_time empty" );
            return FALSE;
        }
        time = remove_timezone_offset(date);
        astro->moonrise= parse_timestring(time, moon_format, TRUE);
        moon_rises = TRUE;
        weather_debug("astro->moonrise=%s\n",
                      format_date(astro->moonrise, NULL,TRUE));
        g_free(time);
    }

    jmoonset = json_object_object_get(jproperties, "moonset");
    if (G_UNLIKELY(jmoonset == NULL)) {
        weather_debug("moonset not found" );
        return FALSE;
    }
    jmoonset_time = json_object_object_get(jmoonset, "time");
    if (G_UNLIKELY(jmoonset_time == NULL)) {
        weather_debug("moonset time not found" );
    } else {
        date = json_object_get_string(jmoonset_time);
        if (G_UNLIKELY(date == NULL)) {
            weather_debug("moonset time empty" );
            return FALSE;
        }
        time = remove_timezone_offset(date);
        astro->moonset= parse_timestring(time, moon_format, TRUE);
        moon_sets = TRUE;
        weather_debug("astro->moonset=%s\n",
                      format_date(astro->moonset, NULL,TRUE));
        g_free(time);
    }

    jmoonphase = json_object_object_get(jproperties, "moonphase");
    if (G_UNLIKELY(jmoonphase == NULL)) {
        weather_debug("moonphase not found" );
        return FALSE;
    }
    astro->moon_phase =g_strdup(parse_moonposition( json_object_get_double(jmoonphase)));
    weather_debug("astro->moonphase=%s\n",astro->moon_phase);

    if (moon_rises)
        astro->moon_never_rises = FALSE;
    else
        astro->moon_never_rises = TRUE;
    if (moon_sets)
        astro->moon_never_sets = FALSE;
    else
        astro->moon_never_sets = TRUE;

    merge_astro(astrodata, astro);
    return TRUE;
}


xml_geolocation *
parse_geolocation(xmlNode *cur_node)
{
    xml_geolocation *geo;

    g_assert(cur_node != NULL);
    if (G_UNLIKELY(cur_node == NULL))
        return NULL;

    geo = g_slice_new0(xml_geolocation);
    if (G_UNLIKELY(geo == NULL))
        return NULL;

    for (cur_node = cur_node->children; cur_node;
         cur_node = cur_node->next) {
        if (NODE_IS_TYPE(cur_node, "City"))
            geo->city = DATA(cur_node);
        if (NODE_IS_TYPE(cur_node, "CountryName"))
            geo->country_name = DATA(cur_node);
        if (NODE_IS_TYPE(cur_node, "CountryCode"))
            geo->country_code = DATA(cur_node);
        if (NODE_IS_TYPE(cur_node, "RegionName"))
            geo->region_name = DATA(cur_node);
        if (NODE_IS_TYPE(cur_node, "Latitude"))
            geo->latitude = DATA(cur_node);
        if (NODE_IS_TYPE(cur_node, "Longitude"))
            geo->longitude = DATA(cur_node);
    }
    return geo;
}


xml_place *
parse_place(xmlNode *cur_node)
{
    xml_place *place;

    g_assert(cur_node != NULL);
    if (G_UNLIKELY(cur_node == NULL || !NODE_IS_TYPE(cur_node, "place")))
        return NULL;

    place = g_slice_new0(xml_place);
    if (G_UNLIKELY(place == NULL))
        return NULL;
    place->lat = PROP(cur_node, "lat");
    place->lon = PROP(cur_node, "lon");
    place->display_name = PROP(cur_node, "display_name");
    return place;
}


xml_altitude *
parse_altitude(xmlNode *cur_node)
{
    xml_altitude *alt;

    g_assert(cur_node != NULL);
    if (G_UNLIKELY(cur_node == NULL) || !NODE_IS_TYPE(cur_node, "geonames"))
        return NULL;

    alt = g_slice_new0(xml_altitude);
    if (G_UNLIKELY(alt == NULL))
        return NULL;
    for (cur_node = cur_node->children; cur_node;
         cur_node = cur_node->next)
        if (NODE_IS_TYPE(cur_node, "srtm3"))
            alt->altitude = DATA(cur_node);
    return alt;
}


xml_timezone *
parse_timezone(xmlNode *cur_node)
{
    xml_timezone *tz;

    g_assert(cur_node != NULL);
    if (G_UNLIKELY(cur_node == NULL) || !NODE_IS_TYPE(cur_node, "geonames"))
        return NULL;

    for (cur_node = cur_node->children; cur_node;
         cur_node = cur_node->next)
        if (NODE_IS_TYPE(cur_node, "timezone"))
            break;

    if (G_UNLIKELY(cur_node == NULL))
        return NULL;

    tz = g_slice_new0(xml_timezone);
    if (G_UNLIKELY(tz == NULL))
        return NULL;

    for (cur_node = cur_node->children; cur_node;
         cur_node = cur_node->next) {
        if (NODE_IS_TYPE(cur_node, "countryCode"))
            tz->country_code = DATA(cur_node);
        if (NODE_IS_TYPE(cur_node, "countryName"))
            tz->country_name = DATA(cur_node);
        if (NODE_IS_TYPE(cur_node, "timezoneId"))
            tz->timezone_id = DATA(cur_node);
    }
    return tz;
}


xmlDoc *
get_xml_document(const gchar *data, gsize len)
{
    if (G_LIKELY(data && len)) {
        if (g_utf8_validate(data, len, NULL)) {
            /* force parsing as UTF-8, the XML encoding header may lie */
            return xmlReadMemory(data, len,
                                 NULL, "UTF-8", 0);
        } else {
            return xmlParseMemory(data, len);
        }
    }
    return NULL;
}

json_object *
get_json_tree(const gchar *data, gsize len)
{
    json_object *res=NULL;
    struct json_tokener *tok = json_tokener_new();

    if (G_UNLIKELY(tok == NULL)) {
        return NULL;
    } else if (G_LIKELY(data && len)) {
        res =  json_tokener_parse_ex(tok, data, len);
        if (res == NULL)
            g_warning("get_json_tree: error =%d",
                      json_tokener_get_error(tok));
    }
    json_tokener_free(tok);
    return res;
}

gpointer
parse_xml_document(const gchar *data, gsize len,
                   XmlParseFunc parse_func)
{
    xmlDoc *doc;
    xmlNode *root_node;
    gpointer user_data = NULL;

    g_assert(data != NULL);
    if (G_UNLIKELY(data == NULL || len == 0))
        return NULL;

    doc = get_xml_document(data, len);
    if (G_LIKELY(doc)) {
        root_node = xmlDocGetRootElement(doc);
        if (G_LIKELY(root_node))
            user_data = parse_func(root_node);
        xmlFreeDoc(doc);
    }
    return user_data;
}


static void
xml_location_free(xml_location *loc)
{
    g_assert(loc != NULL);
    if (G_UNLIKELY(loc == NULL))
        return;
    g_free(loc->altitude);
    g_free(loc->latitude);
    g_free(loc->longitude);
    g_free(loc->temperature_value);
    g_free(loc->temperature_unit);
    g_free(loc->wind_dir_deg);
    g_free(loc->wind_dir_name);
    g_free(loc->wind_speed_mps);
    g_free(loc->wind_speed_beaufort);
    g_free(loc->humidity_value);
    g_free(loc->humidity_unit);
    g_free(loc->pressure_value);
    g_free(loc->pressure_unit);
    g_free(loc->clouds_percent[CLOUDS_PERC_LOW]);
    g_free(loc->clouds_percent[CLOUDS_PERC_MID]);
    g_free(loc->clouds_percent[CLOUDS_PERC_HIGH]);
    g_free(loc->clouds_percent[CLOUDS_PERC_CLOUDINESS]);
    g_free(loc->fog_percent);
    g_free(loc->precipitation_value);
    g_free(loc->precipitation_unit);
    g_free(loc->symbol);
    g_slice_free(xml_location, loc);
}


/*
 * Deep copy xml_astro struct.
 */
xml_astro *
xml_astro_copy(const xml_astro *src)
{
    xml_astro *dst;

    if (G_UNLIKELY(src == NULL))
        return NULL;

    dst = g_slice_new0(xml_astro);
    g_assert(dst != NULL);
    if (G_UNLIKELY(dst == NULL))
        return NULL;

    dst->day = src->day;
    dst->sunrise = src->sunrise;
    dst->sunset = src->sunset;
    dst->sun_never_rises = src->sun_never_rises;
    dst->sun_never_sets = src->sun_never_sets;
    dst->moonrise = src->moonrise;
    dst->moonset = src->moonset;
    dst->moon_never_rises = src->moon_never_rises;
    dst->moon_never_sets = src->moon_never_sets;
    dst->moon_phase = g_strdup(src->moon_phase);
    dst->solarnoon_elevation = src->solarnoon_elevation;
    dst->solarmidnight_elevation = src->solarmidnight_elevation;
    return dst;
}


/*
 * Deep copy xml_time struct.
 */
xml_time *
xml_time_copy(const xml_time *src)
{
    xml_time *dst;
    xml_location *loc;
    gint i;

    if (G_UNLIKELY(src == NULL))
        return NULL;

    dst = g_slice_new0(xml_time);
    g_assert(dst != NULL);
    if (G_UNLIKELY(dst == NULL))
        return NULL;

    loc = g_slice_new0(xml_location);
    g_assert(loc != NULL);
    if (loc == NULL) {
        g_slice_free(xml_time, dst);
        return NULL;
    }

    dst->start = src->start;
    dst->end = src->end;

    loc->altitude = g_strdup(src->location->altitude);
    loc->latitude = g_strdup(src->location->latitude);
    loc->longitude = g_strdup(src->location->longitude);

    loc->temperature_value = g_strdup(src->location->temperature_value);
    loc->temperature_unit = g_strdup(src->location->temperature_unit);

    loc->wind_dir_deg = g_strdup(src->location->wind_dir_deg);
    loc->wind_dir_name = g_strdup(src->location->wind_dir_name);
    loc->wind_speed_mps = g_strdup(src->location->wind_speed_mps);
    loc->wind_speed_beaufort = g_strdup(src->location->wind_speed_beaufort);

    loc->humidity_value = g_strdup(src->location->humidity_value);
    loc->humidity_unit = g_strdup(src->location->humidity_unit);

    loc->pressure_value = g_strdup(src->location->pressure_value);
    loc->pressure_unit = g_strdup(src->location->pressure_unit);

    for (i = 0; i < CLOUDS_PERC_NUM; i++)
        loc->clouds_percent[i] = g_strdup(src->location->clouds_percent[i]);

    loc->fog_percent = g_strdup(src->location->fog_percent);

    loc->precipitation_value =
        g_strdup(src->location->precipitation_value);
    loc->precipitation_unit = g_strdup(src->location->precipitation_unit);

    loc->symbol_id = src->location->symbol_id;
    loc->symbol = g_strdup(src->location->symbol);

    dst->location = loc;

    return dst;
}


void
xml_time_free(xml_time *timeslice)
{
    g_assert(timeslice != NULL);
    if (G_UNLIKELY(timeslice == NULL))
        return;
    xml_location_free(timeslice->location);
    g_slice_free(xml_time, timeslice);
}


void
xml_weather_free(xml_weather *wd)
{
    xml_time *timeslice;
    guint i;

    g_assert(wd != NULL);
    if (G_UNLIKELY(wd == NULL))
        return;
    if (G_LIKELY(wd->timeslices)) {
        weather_debug("Freeing %u timeslices.", wd->timeslices->len);
        for (i = 0; i < wd->timeslices->len; i++) {
            timeslice = g_array_index(wd->timeslices, xml_time *, i);
            xml_time_free(timeslice);
        }
        g_array_free(wd->timeslices, FALSE);
    }
    if (G_LIKELY(wd->current_conditions)) {
        weather_debug("Freeing current conditions.");
        xml_time_free(wd->current_conditions);
    }
    g_slice_free(xml_weather, wd);
}


void
xml_weather_clean(xml_weather *wd)
{
    xml_time *timeslice;
    time_t now_t = time(NULL);
    guint i;

    if (G_UNLIKELY(wd == NULL || wd->timeslices == NULL))
        return;
    for (i = 0; i < wd->timeslices->len; i++) {
        timeslice = g_array_index(wd->timeslices, xml_time *, i);
        if (G_UNLIKELY(timeslice == NULL))
            continue;
        if (difftime(now_t, timeslice->end) > DATA_EXPIRY_TIME) {
            weather_debug("Removing expired timeslice:");
            weather_dump(weather_dump_timeslice, timeslice);
            xml_time_free(timeslice);
            g_array_remove_index(wd->timeslices, i--);
            weather_debug("Remaining timeslices: %d", wd->timeslices->len);
        }
    }
}


void
xml_astro_free(xml_astro *astro)
{
    g_assert(astro != NULL);
    if (G_UNLIKELY(astro == NULL))
        return;
    g_free(astro->moon_phase);
    g_slice_free(xml_astro, astro);
}


void
astrodata_free(GArray *astrodata)
{
    xml_astro *astro;
    guint i;

    if (G_UNLIKELY(astrodata == NULL))
        return;
    for (i = 0; i < astrodata->len; i++) {
        astro = g_array_index(astrodata, xml_astro *, i);
        if (astro)
            xml_astro_free(astro);
    }
    g_array_free(astrodata, FALSE);
}


void
xml_geolocation_free(xml_geolocation *geo)
{
    g_assert(geo != NULL);
    if (G_UNLIKELY(geo == NULL))
        return;
    g_free(geo->city);
    g_free(geo->country_name);
    g_free(geo->country_code);
    g_free(geo->region_name);
    g_free(geo->latitude);
    g_free(geo->longitude);
    g_slice_free(xml_geolocation, geo);
}


void
xml_place_free(xml_place *place)
{
    g_assert(place != NULL);
    if (G_UNLIKELY(place == NULL))
        return;
    g_free(place->lat);
    g_free(place->lon);
    g_free(place->display_name);
    g_slice_free(xml_place, place);
}


void
xml_altitude_free(xml_altitude *alt)
{
    g_assert(alt != NULL);
    if (G_UNLIKELY(alt == NULL))
        return;
    g_free(alt->altitude);
    g_slice_free(xml_altitude, alt);
}


void
xml_timezone_free(xml_timezone *tz)
{
    g_assert(tz != NULL);
    if (G_UNLIKELY(tz == NULL))
        return;
    g_free(tz->country_code);
    g_free(tz->country_name);
    g_free(tz->timezone_id);
    g_slice_free(xml_timezone, tz);
}
