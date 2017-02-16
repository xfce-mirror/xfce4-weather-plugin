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
    DEWPOINT,
    APPARENT_TEMPERATURE,
    CLOUDS_LOW,
    CLOUDS_MID,
    CLOUDS_HIGH,
    CLOUDINESS,
    FOG,
    PRECIPITATION,
    SYMBOL
} data_types;

typedef enum {
    CELSIUS,
    FAHRENHEIT
} units_temperature;

typedef enum {
    HECTOPASCAL,
    INCH_MERCURY,
    PSI,
    TORR
} units_pressure;

typedef enum {
    KMH,
    MPH,
    MPS,
    FTS,
    KNOTS
} units_windspeed;

typedef enum {
    MILLIMETERS,
    INCHES
} units_precipitation;

typedef enum {
    METERS,
    FEET
} units_altitude;

typedef enum {
    WINDCHILL_HEATINDEX,
    WINDCHILL_HUMIDEX,
    STEADMAN,
    QUAYLE_STEADMAN
} apparent_temp_models;

typedef enum {
    MORNING,
    AFTERNOON,
    EVENING,
    NIGHT
} daytime;

typedef struct {
    gint temperature;
    gint apparent_temperature;
    gint pressure;
    gint windspeed;
    gint precipitation;
    gint altitude;
} units_config;


gdouble string_to_double(const gchar *str,
                         gdouble backup);

gchar *double_to_string(gdouble val,
                        const gchar *format);

gchar *format_date(const time_t t,
                   gchar *format,
                   gboolean local);

gboolean timeslice_is_interval(xml_time *timeslice);

gchar *get_data(const xml_time *timeslice,
                const units_config *units,
                data_types type,
                gboolean round,
                gboolean night_time);

const gchar *get_unit(const units_config *units,
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

gint xml_astro_compare(gconstpointer a,
                       gconstpointer b);

gint xml_time_compare(gconstpointer a,
                      gconstpointer b);

void merge_astro(GArray *astrodata,
                 const xml_astro *astro);

void astrodata_clean(GArray *astrodata);

void merge_timeslice(xml_weather *wd,
                     const xml_time *timeslice);

xml_time *get_current_conditions(const xml_weather *wd);

xml_time *make_current_conditions(xml_weather *wd,
                                  time_t now_t);

time_t day_at_midnight(time_t day_t,
                       const gint add_days);

xml_astro *get_astro_data_for_day(const GArray *astrodata,
                                  const gint day);

GArray *get_point_data_for_day(xml_weather *wd,
                               const gint day);

xml_time *make_forecast_data(xml_weather *wd,
                             GArray *daydata,
                             gint day,
                             daytime dt);

G_END_DECLS

#endif
