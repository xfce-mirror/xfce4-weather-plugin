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

#ifndef __WEATHER_PARSERS_H__
#define __WEATHER_PARSERS_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <libsoup/soup.h>

#define DATA_EXPIRY_TIME (24 * 3600)

G_BEGIN_DECLS

enum {
    CLOUDS_PERC_LOW = 0,
    CLOUDS_PERC_MID,
    CLOUDS_PERC_HIGH,
    CLOUDS_PERC_CLOUDINESS,
    CLOUDS_PERC_NUM
};

typedef gpointer (*XmlParseFunc) (xmlNode *node);

typedef struct {
    gchar *altitude;
    gchar *latitude;
    gchar *longitude;

    gchar *temperature_value;
    gchar *temperature_unit;

    gchar *wind_dir_deg;
    gchar *wind_dir_name;
    gchar *wind_speed_mps;
    gchar *wind_speed_beaufort;

    gchar *humidity_value;
    gchar *humidity_unit;

    gchar *pressure_value;
    gchar *pressure_unit;

    gchar *clouds_percent[CLOUDS_PERC_NUM];
    gchar *fog_percent;

    gchar *precipitation_value;
    gchar *precipitation_unit;

    gint symbol_id;
    gchar *symbol;
} xml_location;

typedef struct {
    time_t start;
    time_t end;
    time_t point;
    xml_location *location;
} xml_time;

typedef struct {
    GArray *timeslices;
    xml_time *current_conditions;
} xml_weather;

typedef struct {
    time_t day;

    time_t sunrise;
    time_t sunset;
    gboolean sun_never_rises;
    gboolean sun_never_sets;

    time_t moonrise;
    time_t moonset;
    gboolean moon_never_rises;
    gboolean moon_never_sets;
    gchar *moon_phase;
} xml_astro;

typedef struct {
    gchar *city;
    gchar *country_name;
    gchar *country_code;
    gchar *region_name;
    gchar *latitude;
    gchar *longitude;
} xml_geolocation;

typedef struct {
    gchar *display_name;
    gchar *lat;
    gchar *lon;
} xml_place;

typedef struct {
    gchar *altitude;
} xml_altitude;

typedef struct {
    gchar *country_code;
    gchar *country_name;
    gchar *timezone_id;
} xml_timezone;


xml_weather *make_weather_data(void);

xml_time *make_timeslice(void);

time_t parse_timestring(const gchar *ts,
                        gchar *format,
                        gboolean local);

gboolean parse_weather(xmlNode *cur_node,
                       xml_weather *wd);

xml_astro *parse_astro(xmlNode *cur_node);

gboolean parse_astrodata(xmlNode *cur_node,
                         GArray *astrodata);

xml_geolocation *parse_geolocation(xmlNode *cur_node);

xml_place *parse_place(xmlNode *cur_node);

xml_altitude *parse_altitude(xmlNode *cur_node);

xml_timezone *parse_timezone(xmlNode *cur_node);

xml_time *get_timeslice(xml_weather *wd,
                        const time_t start_t,
                        const time_t end_t,
                        guint *index);

xml_astro *get_astro(const GArray *astrodata,
                     const time_t day_t,
                     guint *index);

xmlDoc *get_xml_document(SoupMessage *msg);

gpointer parse_xml_document(SoupMessage *msg,
                            XmlParseFunc parse_func);

xml_astro *xml_astro_copy(const xml_astro *src);

xml_time *xml_time_copy(const xml_time *src);

void xml_time_free(xml_time *timeslice);

void xml_weather_free(xml_weather *wd);

void xml_weather_clean(xml_weather *wd);

void xml_astro_free(xml_astro *astro);

void astrodata_free(GArray *astrodata);

void xml_geolocation_free(xml_geolocation *geo);

void xml_place_free(xml_place *place);

void xml_altitude_free(xml_altitude *alt);

void xml_timezone_free(xml_timezone *tz);

G_END_DECLS

#endif
