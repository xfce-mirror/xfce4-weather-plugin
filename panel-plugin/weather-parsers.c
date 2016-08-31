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
get_timeslice(xml_weather *wd,
              const time_t start_t,
              const time_t end_t,
              guint *index)
{
    xml_time *timeslice;
    gint i;

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
    gint i;

    g_assert(astrodata != NULL);
    if (G_UNLIKELY(astrodata == NULL))
        return NULL;

    for (i = 0; i < astrodata->len; i++) {
        astro = g_array_index(astrodata, xml_astro *, i);
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
                 gchar *format,
                 gboolean local) {
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

    if (local)
        t = mktime(&tm);
    else
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
            astro->sunrise = parse_timestring(sunrise, NULL, FALSE);
            xmlFree(sunrise);

            sunset = PROP(child_node, "set");
            astro->sunset = parse_timestring(sunset, NULL, FALSE);
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
            astro->moonrise = parse_timestring(moonrise, NULL, FALSE);
            xmlFree(moonrise);

            moonset = PROP(child_node, "set");
            astro->moonset = parse_timestring(moonset, NULL, FALSE);
            xmlFree(moonset);

            astro->moon_phase = PROP(child_node, "phase");
        }
    }
}


static xml_astro *
parse_astro_time(xmlNode *cur_node)
{
    xmlNode *child_node;
    xml_astro *astro;
    gchar *date;

    astro = g_slice_new0(xml_astro);
    if (G_UNLIKELY(astro == NULL))
        return NULL;

    date = PROP(cur_node, "date");
    astro->day = parse_timestring(date, "%Y-%m-%d", TRUE);
    xmlFree(date);

    for (child_node = cur_node->children; child_node;
         child_node = child_node->next)
        if (NODE_IS_TYPE(child_node, "location"))
            parse_astro_location(child_node, astro);
    return astro;
}


/*
 * Look at https://api.met.no/weatherapi/sunrise/1.1/schema for information
 * of elements and attributes to expect.
 */
gboolean
parse_astrodata(xmlNode *cur_node,
                GArray *astrodata)
{
    xmlNode *child_node;
    xml_astro *astro;

    g_assert(astrodata != NULL);
    if (G_UNLIKELY(astrodata == NULL))
        return FALSE;

    g_assert(cur_node != NULL);
    if (G_UNLIKELY(cur_node == NULL || !NODE_IS_TYPE(cur_node, "astrodata")))
        return FALSE;

    for (child_node = cur_node->children; child_node;
         child_node = child_node->next)
        if (NODE_IS_TYPE(child_node, "time")) {
            if ((astro = parse_astro_time(child_node))) {
                merge_astro(astrodata, astro);
                xml_astro_free(astro);
            }
        }
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
get_xml_document(SoupMessage *msg)
{
    if (G_LIKELY(msg && msg->response_body && msg->response_body->data))
        if (g_utf8_validate(msg->response_body->data, -1, NULL)) {
            /* force parsing as UTF-8, the XML encoding header may lie */
            return xmlReadMemory(msg->response_body->data,
                                 strlen(msg->response_body->data),
                                 NULL, "UTF-8", 0);
        } else
            return xmlParseMemory(msg->response_body->data,
                                  strlen(msg->response_body->data));
    return NULL;
}


gpointer
parse_xml_document(SoupMessage *msg,
                   XmlParseFunc parse_func)
{
    xmlDoc *doc;
    xmlNode *root_node;
    gpointer user_data = NULL;

    g_assert(msg != NULL);
    if (G_UNLIKELY(msg == NULL))
        return NULL;

    doc = get_xml_document(msg);
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
    gint i;

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
    gint i;

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
    gint i;

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
