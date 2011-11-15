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

#include <libxfce4util/libxfce4util.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#define CHK_NULL(s) ((s) ? (s):"")

const gchar *
get_data (xml_weather *data, datas type)
{
	const xml_time *timeslice = NULL;
	const xml_location *loc = NULL;

	if (data == NULL)
		return "";

	timeslice = get_current_timeslice(data);
	if (timeslice == NULL)
		return "";

	loc = timeslice->location;

	switch(type) {
	case TEMPERATURE:
		return CHK_NULL(loc->temperature_value);
	case PRESSURE:
		return CHK_NULL(loc->pressure_value);
	case WIND_SPEED:
		return CHK_NULL( loc->wind_speed_mps);
	case WIND_DIRECTION:
		return CHK_NULL(loc->wind_dir_name);
	case HUMIDITY:
		return CHK_NULL(loc->humidity_value);
	case CLOUDINESS:
		return CHK_NULL(loc->cloudiness_percent[CLOUDINESS_LOW]);
	case FOG:
		return CHK_NULL(loc->fog_percent);
	case PRECIPITATIONS:
		return CHK_NULL(loc->precipitation_value);
	case SYMBOL:
		return CHK_NULL(loc->symbol);
	}
	return "";
}

const gchar *
get_unit (xml_weather *data, units unit, datas type)
{
	const xml_time *timeslice = NULL;
	const xml_location *loc = NULL;

	if (data == NULL)
		return "";

	timeslice = get_current_timeslice(data);
	if (timeslice == NULL)
		return "";

	loc = timeslice->location;

	switch(type) {
	case TEMPERATURE:
		return strcmp(loc->temperature_unit, "celcius") ? "°F":"°C";
	case PRESSURE:
		return CHK_NULL(loc->pressure_unit);
	case WIND_SPEED:
		return "m/s";
	case WIND_DIRECTION:
		return "";
	case HUMIDITY:
		return "%";
	case CLOUDINESS:
		return "%";
	case FOG:
		return "%";
	case PRECIPITATIONS:
		return "mm";
	case SYMBOL:
		return "";
	}
	return "";
}

