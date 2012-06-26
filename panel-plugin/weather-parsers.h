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
#define DATA(node) (gchar *) xmlNodeListGetString(node->doc, node->children, 1)
#define PROP(node, prop) ((gchar *) xmlGetProp ((node), (const xmlChar *) (prop)))
#define NODE_IS_TYPE(node, type) xmlStrEqual (node->name, (const xmlChar *) type)
#define MAX_TIMESLICE 250

enum
{
	CLOUD_LOW = 0,
	CLOUD_MED,
	CLOUD_HIGH,
	NUM_CLOUDINESS
};

typedef struct
{
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
	
	gchar *cloudiness_percent[NUM_CLOUDINESS];
	gchar *fog_percent;
	
	gchar *precipitation_value;
	gchar *precipitation_unit;

	gint   symbol_id;
	gchar *symbol;
}
xml_location;

typedef struct
{
	time_t start;
	time_t end;
	xml_location *location;
}
xml_time;

typedef struct
{
  xml_time *timeslice[MAX_TIMESLICE];
  guint num_timeslices;
}
xml_weather;

xml_weather *parse_weather (xmlNode * cur_node);

void parse_time (xmlNode * cur_node, xml_weather * data);

void parse_location (xmlNode * cur_node, xml_location *location);

xml_time *get_timeslice(xml_weather *data, time_t start, time_t end);
xml_time *get_current_timeslice(xml_weather *data, gboolean interval);

void xml_time_free(xml_time *timeslice);
void xml_weather_free (xml_weather * data);

G_END_DECLS
#endif
