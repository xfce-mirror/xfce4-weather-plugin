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

#ifndef __WEATHER_DATA_H__
#define __WEATHER_DATA_H__

G_BEGIN_DECLS

typedef enum {
    ALTITUDE,
    LATITUDE,
    LONGITUDE,
    TEMPERATURE,
    PRESSURE,
    WIND_SPEED,
    WIND_BEAUFORT,
    WIND_DIRECTION,
    WIND_DIRECTION_DEG,
    HUMIDITY,
    CLOUDS_LOW,
    CLOUDS_MED,
    CLOUDS_HIGH,
    CLOUDINESS,
    FOG,
    PRECIPITATIONS,
    SYMBOL,
    TEMPERATURE_RAW
} data_types;

typedef enum {
    IMPERIAL,
    METRIC
} unit_systems;

typedef enum {
    MORNING,
    AFTERNOON,
    EVENING,
    NIGHT
} daytime;


gchar *get_data(const xml_time *timeslice,
                unit_systems unit_system,
                data_types type);

const gchar *get_unit(unit_systems unit_system,
                      data_types type);

gboolean is_night_time(const xml_astro *astro);

time_t time_calc(struct tm time_tm,
                 gint year,
                 gint mon,
                 gint day,
                 gint hour,
                 gint min,
                 gint sec);

time_t time_calc_hour(struct tm time_tm,
                      gint hours);

time_t time_calc_day(struct tm time_tm,
                     gint days);

xml_time *get_current_conditions(const xml_weather *data);

xml_time *make_current_conditions(xml_weather *data);

xml_time *make_forecast_data(xml_weather *data,
                             int day,
                             daytime dt);

G_END_DECLS

#endif
