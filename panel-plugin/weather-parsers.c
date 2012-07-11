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

#include "weather-parsers.h"
#include <libxfce4panel/libxfce4panel.h>
#define _XOPEN_SOURCE
#include <time.h>
#include <stdlib.h>

static time_t my_timegm(struct tm *tm)
{
	time_t ret;
	char *tz;

	tz = getenv("TZ");
	setenv("TZ", "", 1);
	tzset();
	ret = mktime(tm);
	if (tz)
		setenv("TZ", tz, 1);
	else
		unsetenv("TZ");
	tzset();
	return ret;
}

xml_weather *
parse_weather (xmlNode *cur_node)
{
  xml_weather *ret;
  xmlNode     *child_node;
  guint        i = 0;

  if (!NODE_IS_TYPE (cur_node, "weatherdata"))
    {
      return NULL;
    }

  if ((ret = g_slice_new0 (xml_weather)) == NULL)
    return NULL;

  ret->num_timeslices = 0;
  for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
      if (cur_node->type != XML_ELEMENT_NODE)
        continue;

      if (NODE_IS_TYPE (cur_node, "product"))
        {
	  gchar *class = xmlGetProp (cur_node, (const xmlChar *) "class");
	  if (xmlStrcasecmp(class, "pointData")) {
		xmlFree(class);
		continue;
	  }
	  g_free(class);
          for (child_node = cur_node->children; child_node;
               child_node = child_node->next)
            {
              if (NODE_IS_TYPE (child_node, "time"))
                {
                  parse_time(child_node, ret);
                }
            }
        }
    }

  return ret;
}

void parse_time (xmlNode * cur_node, xml_weather * data) {
	gchar *datatype = xmlGetProp (cur_node, (const xmlChar *) "datatype");
	gchar *start = xmlGetProp (cur_node, (const xmlChar *) "from");
	gchar *end = xmlGetProp (cur_node, (const xmlChar *) "to");
	struct tm start_t, end_t;
	time_t start_ts, end_ts;
	time_t cur_ts;
	xml_time *timeslice;
	xmlNode *child_node;

	if (xmlStrcasecmp(datatype, "forecast")) {
		xmlFree(datatype);
		xmlFree(start);
		xmlFree(end);
		return;
	}

	xmlFree(datatype);

	if (strptime(start, "%Y-%m-%dT%H:%M:%SZ", &start_t) == NULL) {
		xmlFree(start);
		xmlFree(end);
		return;
	}

	if (strptime(end, "%Y-%m-%dT%H:%M:%SZ", &end_t) == NULL) {
		xmlFree(start);
		xmlFree(end);
		return;
	}

	xmlFree(start);
	xmlFree(end);

	start_ts = my_timegm(&start_t);
	end_ts = my_timegm(&end_t);
	
	timeslice = get_timeslice(data, start_ts, end_ts);
	
	if (!timeslice) {
		g_warning("no timeslice");
		return;
	}
	for (child_node = cur_node->children; child_node;
	     child_node = child_node->next) {
		if (NODE_IS_TYPE (child_node, "location")) {
			if (timeslice->location == NULL)
				timeslice->location =
					g_slice_new0(xml_location);
			parse_location(child_node, timeslice->location);
		}
	}
}

xml_time *get_timeslice(xml_weather *data, time_t start, time_t end)
{
	int i;
	for (i = 0; i < data->num_timeslices; i++) {
		if (data->timeslice[i]->start == start
		 && data->timeslice[i]->end == end)
			return data->timeslice[i];
	}
	if (data->num_timeslices == MAX_TIMESLICE -1)
		return NULL;

	data->timeslice[data->num_timeslices] = g_slice_new0(xml_time);
	data->timeslice[data->num_timeslices]->start = start;
	data->timeslice[data->num_timeslices]->end = end;
	data->num_timeslices++;

	return data->timeslice[data->num_timeslices - 1];
}

xml_time *make_current_conditions(xml_weather *data)
{
    xml_time *forecast, *point_data, *interval_data;
    struct tm tm_now, tm_start, tm_end;
    time_t now, start_t, end_t;
    gint interval;

    /* get the current time */
    time(&now);
    tm_now = *localtime(&now);

    /* find nearest point data, starting with the current hour, with a
     * deviation of 1 hour into the past and 6 hours into the future */
    point_data = find_timeslice(data, tm_now, tm_now, -1, 6);
    if (point_data == NULL)
        return NULL;

    /* now search for the nearest and shortest interval data
     * available, using a maximum interval of 6 hours */
    tm_end = tm_start = tm_now;
    start_t = mktime(&tm_start);

    /* set interval to 1 hour as minimum, we don't want to retrieve point data */
    end_t = time_calc_hour(tm_end, 1);
    tm_end = *localtime(&end_t);

    /* We want to keep the hour deviation as small as possible,
     * so let's try an interval with ±1 hour deviation first */
    interval_data = find_shortest_timeslice(data, tm_start, tm_end, -1, 1, 6);
    if (interval_data == NULL) {
        /* in case we were unsuccessful we might need to enlarge the search radius */
        interval_data = find_shortest_timeslice(data, tm_start, tm_end, -3, 3, 6);
        if (interval_data == NULL)
            /* and maybe it's necessary to try even harder... */
            interval_data = find_shortest_timeslice(data, tm_start, tm_end, -3, 6, 6);
    }
    if (interval_data == NULL)
        return NULL;

    /* create a new timeslice with combined point and interval data */
    forecast = make_combined_timeslice(point_data, interval_data);
    return forecast;
}

void parse_location (xmlNode * cur_node, xml_location *loc)
{
	xmlNode *child_node;
	gchar *altitude, *latitude, *longitude;

	g_free(loc->altitude);
	loc->altitude = PROP(cur_node, "altitude");
	xmlFree(altitude);

	g_free(loc->latitude);
	loc->latitude = PROP(cur_node, "latitude");
	xmlFree(latitude);

	g_free(loc->longitude);
	loc->longitude = PROP(cur_node, "longitude");
	xmlFree(longitude);

	for (child_node = cur_node->children; child_node;
	     child_node = child_node->next) {
		if (NODE_IS_TYPE (child_node, "temperature")) {
			g_free(loc->temperature_unit);
			g_free(loc->temperature_value);
			loc->temperature_unit = PROP(child_node, "unit");
			loc->temperature_value = PROP(child_node, "value");
		}
		if (NODE_IS_TYPE (child_node, "windDirection")) {
			g_free(loc->wind_dir_deg);
			g_free(loc->wind_dir_name);
			loc->wind_dir_deg = PROP(child_node, "deg");
			loc->wind_dir_name = PROP(child_node, "name");
		}
		if (NODE_IS_TYPE (child_node, "windSpeed")) {
			g_free(loc->wind_speed_mps);
			g_free(loc->wind_speed_beaufort);
			loc->wind_speed_mps = PROP(child_node, "mps");
			loc->wind_speed_beaufort = PROP(child_node, "beaufort");
		}
		if (NODE_IS_TYPE (child_node, "humidity")) {
			g_free(loc->humidity_unit);
			g_free(loc->humidity_value);
			loc->humidity_unit = PROP(child_node, "unit");
			loc->humidity_value = PROP(child_node, "value");
		}
		if (NODE_IS_TYPE (child_node, "pressure")) {
			g_free(loc->pressure_unit);
			g_free(loc->pressure_value);
			loc->pressure_unit = PROP(child_node, "unit");
			loc->pressure_value = PROP(child_node, "value");
		}
		if (NODE_IS_TYPE (child_node, "cloudiness")) {
			g_free(loc->cloudiness_percent[CLOUD_OVERALL]);
			loc->cloudiness_percent[CLOUD_OVERALL] = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "fog")) {
			g_free(loc->fog_percent);
			loc->fog_percent = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "lowClouds")) {
			g_free(loc->cloudiness_percent[CLOUD_LOW]);
			loc->cloudiness_percent[CLOUD_LOW] = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "mediumClouds")) {
			g_free(loc->cloudiness_percent[CLOUD_MED]);
			loc->cloudiness_percent[CLOUD_MED] = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "highClouds")) {
			g_free(loc->cloudiness_percent[CLOUD_HIGH]);
			loc->cloudiness_percent[CLOUD_HIGH] = PROP(child_node, "percent");
		}
		if (NODE_IS_TYPE (child_node, "precipitation")) {
			g_free(loc->precipitation_unit);
			g_free(loc->precipitation_value);
			loc->precipitation_unit = PROP(child_node, "unit");
			loc->precipitation_value = PROP(child_node, "value");
		}
		if (NODE_IS_TYPE (child_node, "symbol")) {
			g_free(loc->symbol);
			loc->symbol = PROP(child_node, "id");
			loc->symbol_id = strtol(PROP(child_node, "number"), NULL, 10);
		}
	}
}

static void xml_location_free(xml_location *loc)
{
	g_free(loc->temperature_unit);
	g_free(loc->temperature_value);
	g_free(loc->wind_dir_deg);
	g_free(loc->wind_dir_name);
	g_free(loc->wind_speed_mps);
	g_free(loc->wind_speed_beaufort);
	g_free(loc->humidity_unit);
	g_free(loc->humidity_value);
	g_free(loc->pressure_unit);
	g_free(loc->pressure_value);
	g_free(loc->fog_percent);
	g_free(loc->cloudiness_percent[CLOUD_LOW]);
	g_free(loc->cloudiness_percent[CLOUD_MED]);
	g_free(loc->cloudiness_percent[CLOUD_HIGH]);
	g_free(loc->precipitation_unit);
	g_free(loc->precipitation_value);
	g_free(loc->symbol);
	g_slice_free (xml_location, loc);
}

void xml_time_free(xml_time *timeslice)
{
  xml_location_free(timeslice->location);
  g_slice_free (xml_time, timeslice);
}

void xml_weather_free (xml_weather *data)
{
  guint i;

  for (i = 0; i < data->num_timeslices; i++) {
    xml_time_free(data->timeslice[i]);
  }
  xml_time_free(data->current_conditions);
  g_slice_free (xml_weather, data);
}
