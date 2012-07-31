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

#ifndef __WEATHER_PARSERS_H__
#define __WEATHER_PARSERS_H__

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>

G_BEGIN_DECLS

#define DATA(node)                                                  \
    ((gchar *) xmlNodeListGetString(node->doc, node->children, 1))
#define PROP(node, prop)                                        \
    ((gchar *) xmlGetProp((node), (const xmlChar *) (prop)))
#define NODE_IS_TYPE(node, type)                        \
    (xmlStrEqual(node->name, (const xmlChar *) type))

#define MAX_TIMESLICE 500

enum {
    CLOUDS_PERC_LOW = 0,
    CLOUDS_PERC_MED,
    CLOUDS_PERC_HIGH,
    CLOUDS_PERC_CLOUDINESS,
    CLOUDS_PERC_NUM
};

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
    xml_time *timeslice[MAX_TIMESLICE];
    guint num_timeslices;
    xml_time *current_conditions;
} xml_weather;

typedef struct {
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


xml_weather *parse_weather(xmlNode *cur_node);

xml_astro *parse_astro(xmlNode *cur_node);

void parse_time(xmlNode *cur_node,
                xml_weather *data);

void parse_location(xmlNode *cur_node,
                    xml_location *location);

xml_time *get_timeslice(xml_weather *data,
                        time_t start_t,
                        time_t end_t);

void xml_time_free(xml_time *timeslice);

void xml_weather_free(xml_weather *data);

void xml_astro_free(xml_astro *astro);

G_END_DECLS

#endif
