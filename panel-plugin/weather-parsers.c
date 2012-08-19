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

/*
 * The following two defines fix compile warnings and need to be
 * before time.h and libxfce4panel.h (which includes glib.h).
 * Otherwise, they will be ignored.
 */
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1
#include "weather-parsers.h"
#include <libxfce4panel/libxfce4panel.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "weather-debug.h"


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
    const char *tz;

    tz = g_getenv("TZ");
    g_setenv("TZ", "", 1);
    tzset();
    ret = mktime(tm);
    if (tz)
        g_setenv("TZ", tz, 1);
    else
        g_unsetenv("TZ");
    tzset();
    return ret;
}


xml_time *
get_timeslice(xml_weather *data,
              const time_t start_t,
              const time_t end_t)
{
    guint i;

    for (i = 0; i < data->num_timeslices; i++) {
        if (data->timeslice[i]->start == start_t &&
            data->timeslice[i]->end == end_t)
            return data->timeslice[i];
    }
    if (data->num_timeslices == MAX_TIMESLICE - 1)
        return NULL;

    data->timeslice[data->num_timeslices] = g_slice_new0(xml_time);
    data->timeslice[data->num_timeslices]->start = start_t;
    data->timeslice[data->num_timeslices]->end = end_t;
    data->num_timeslices++;

    return data->timeslice[data->num_timeslices - 1];
}


static time_t
parse_xml_timestring(const gchar *ts,
                     gchar *format) {
    time_t t;
    struct tm tm;

    memset(&t, 0, sizeof(time_t));
    if (G_UNLIKELY(ts == NULL))
        return t;

    /* standard format */
    if (format == NULL)
        format = "%Y-%m-%dT%H:%M:%SZ";

    /* strptime needs an initialized struct, or unpredictable
     * behaviour might occur */
    memset(&tm, 0, sizeof(struct tm));
    tm.tm_isdst = -1;

    if (G_UNLIKELY(strptime(ts, format, &tm) == NULL))
        return t;

    t = my_timegm(&tm);
    return t;
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
            g_free(loc->clouds_percent[CLOUDS_PERC_MED]);
            loc->clouds_percent[CLOUDS_PERC_MED] = PROP(child_node, "percent");
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
            loc->symbol = PROP(child_node, "id");
            loc->symbol_id = strtol(PROP(child_node, "number"), NULL, 10);
        }
    }
}


static void
parse_time(xmlNode *cur_node,
           xml_weather *data)
{
    gchar *datatype, *from, *to;
    time_t start_t, end_t;
    xml_time *timeslice;
    xmlNode *child_node;

    datatype = PROP(cur_node, "datatype");
    if (xmlStrcasecmp(datatype, "forecast")) {
        xmlFree(datatype);
        return;
    }
    xmlFree(datatype);

    from = PROP(cur_node, "from");
    start_t = parse_xml_timestring(from, NULL);
    xmlFree(from);

    to = PROP(cur_node, "to");
    end_t = parse_xml_timestring(to, NULL);
    xmlFree(to);

    if (G_UNLIKELY(!start_t || !end_t))
        return;

    timeslice = get_timeslice(data, start_t, end_t);

    if (G_UNLIKELY(!timeslice)) {
        g_warning("No timeslice found or created. "
                  "Perhaps maximum of %d slices reached?", MAX_TIMESLICE);
        return;
    }
    for (child_node = cur_node->children; child_node;
         child_node = child_node->next)
        if (G_LIKELY(NODE_IS_TYPE(child_node, "location"))) {
            if (timeslice->location == NULL)
                timeslice->location = g_slice_new0(xml_location);
            parse_location(child_node, timeslice->location);
        }
}


xml_weather *
parse_weather(xmlNode *cur_node)
{
    xml_weather *ret;
    xmlNode *child_node;

    if (G_UNLIKELY(!NODE_IS_TYPE(cur_node, "weatherdata"))) {
        return NULL;
    }

    if ((ret = g_slice_new0(xml_weather)) == NULL)
        return NULL;

    ret->num_timeslices = 0;
    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE(cur_node, "product")) {
            gchar *class = PROP(cur_node, "class");
            if (xmlStrcasecmp(class, "pointData")) {
                xmlFree(class);
                continue;
            }
            g_free(class);
            for (child_node = cur_node->children; child_node;
                 child_node = child_node->next)
                if (NODE_IS_TYPE(child_node, "time"))
                    parse_time(child_node, ret);
        }
    }
    return ret;
}


static void
parse_astro_location(xmlNode *cur_node,
                     xml_astro *astro)
{
    xmlNode *child_node;
    gchar *sunrise, *sunset, *moonrise, *moonset;
    gchar *never_rises, *never_sets;

    for (child_node = cur_node->children; child_node;
         child_node = child_node->next) {
        if (NODE_IS_TYPE(child_node, "sun")) {
            never_rises = PROP(child_node, "never_rise");
            if (never_rises &&
                (!strcmp(never_rises, "true") ||
                 !strcmp(never_rises, "1")))
                astro->sun_never_rises = TRUE;
            else
                astro->sun_never_rises = FALSE;
            xmlFree(never_rises);

            never_sets = PROP(child_node, "never_set");
            if (never_sets &&
                (!strcmp(never_sets, "true") ||
                 !strcmp(never_sets, "1")))
                astro->sun_never_sets = TRUE;
            else
                astro->sun_never_sets = FALSE;
            xmlFree(never_sets);

            sunrise = PROP(child_node, "rise");
            astro->sunrise = parse_xml_timestring(sunrise, NULL);
            xmlFree(sunrise);

            sunset = PROP(child_node, "set");
            astro->sunset = parse_xml_timestring(sunset, NULL);
            xmlFree(sunset);
        }

        if (NODE_IS_TYPE(child_node, "moon")) {
            never_rises = PROP(child_node, "never_rise");
            if (never_rises &&
                (!strcmp(never_rises, "true") ||
                 !strcmp(never_rises, "1")))
                astro->moon_never_rises = TRUE;
            else
                astro->moon_never_rises = FALSE;
            xmlFree(never_rises);

            never_sets = PROP(child_node, "never_set");
            if (never_sets &&
                (!strcmp(never_sets, "true") ||
                 !strcmp(never_sets, "1")))
                astro->moon_never_sets = TRUE;
            else
                astro->moon_never_sets = FALSE;
            xmlFree(never_sets);

            moonrise = PROP(child_node, "rise");
            astro->moonrise = parse_xml_timestring(moonrise, NULL);
            xmlFree(moonrise);

            moonset = PROP(child_node, "set");
            astro->moonset = parse_xml_timestring(moonset, NULL);
            xmlFree(moonset);

            astro->moon_phase = PROP(child_node, "phase");
        }
    }
}


/*
 * Look at http://api.yr.no/weatherapi/sunrise/1.0/schema for information
 * of elements and attributes to expect.
 */
xml_astro *
parse_astro(xmlNode *cur_node)
{
    xmlNode *child_node, *time_node = NULL;
    xml_astro *astro;

    if (cur_node == NULL || !NODE_IS_TYPE(cur_node, "astrodata"))
        return NULL;

    astro = g_slice_new0(xml_astro);
    if (G_UNLIKELY(astro == NULL))
        return NULL;

    for (child_node = cur_node->children; child_node;
         child_node = child_node->next)
        if (NODE_IS_TYPE(child_node, "time")) {
            time_node = child_node;
            break;
        }

    if (G_LIKELY(time_node))
        for (child_node = time_node->children; child_node;
             child_node = child_node->next)
            if (NODE_IS_TYPE(child_node, "location"))
                parse_astro_location(child_node, astro);
    return astro;
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
    if (G_UNLIKELY(cur_node == NULL))
        return NULL;

    if (!NODE_IS_TYPE(cur_node, "place"))
        return NULL;

    place = g_slice_new0(xml_place);
    if (G_UNLIKELY(place == NULL))
        return NULL;
    place->lat = PROP(cur_node, "lat");
    place->lon = PROP(cur_node, "lon");
    place->display_name = PROP(cur_node, "display_name");
    return place;
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
    g_free(loc->clouds_percent[CLOUDS_PERC_MED]);
    g_free(loc->clouds_percent[CLOUDS_PERC_HIGH]);
    g_free(loc->clouds_percent[CLOUDS_PERC_CLOUDINESS]);
    g_free(loc->fog_percent);
    g_free(loc->precipitation_value);
    g_free(loc->precipitation_unit);
    g_free(loc->symbol);
    g_slice_free(xml_location, loc);
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
xml_weather_free(xml_weather *data)
{
    guint i;

    g_assert(data != NULL);
    if (G_UNLIKELY(data == NULL))
        return;
    weather_debug("Freeing %u timeslices.", data->num_timeslices);
    for (i = 0; i < data->num_timeslices; i++)
        xml_time_free(data->timeslice[i]);
    if (G_LIKELY(data->current_conditions)) {
        weather_debug("Freeing current conditions.");
        xml_time_free(data->current_conditions);
    }
    g_slice_free(xml_weather, data);
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
}
