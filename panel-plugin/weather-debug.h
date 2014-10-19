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

#ifndef __WEATHER_DEBUG_H__
#define __WEATHER_DEBUG_H__

#include <glib.h>
#include <stdarg.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather-icon.h"
#include "weather.h"

G_BEGIN_DECLS

#if __STDC_VERSION__ < 199901L
#if __GNUC__ >= 2
#define __func__ __FUNCTION__
#else
#define __func__ "<unknown>"
#endif
#endif

#define weather_debug(...)                                  \
    weather_debug_real(G_LOG_DOMAIN, __FILE__, __func__,    \
                       __LINE__, __VA_ARGS__)

#define weather_dump(func, data)                \
    if (G_UNLIKELY(debug_mode)) {               \
        gchar *msg = func(data);                \
        weather_debug("%s", msg);               \
        g_free(msg);                            \
    }

void weather_debug_init(const gchar *log_domain,
                        gboolean debug_mode);

void weather_debug_real(const gchar *log_domain,
                        const gchar *file,
                        const gchar *func,
                        gint line,
                        const gchar *format,
                        ...);

gchar *weather_dump_geolocation(const xml_geolocation *geo);

gchar *weather_dump_place(const xml_place *place);

gchar *weather_dump_timezone(const xml_timezone *timezone);

gchar *weather_dump_icon_theme(const icon_theme *theme);

gchar *weather_dump_astrodata(const GArray *astrodata);

gchar *weather_dump_astro(const xml_astro *astro);

gchar *weather_dump_units_config(const units_config *units);

gchar *weather_dump_timeslice(const xml_time *timeslice);

gchar *weather_dump_weatherdata(const xml_weather *wd);

gchar *weather_dump_plugindata(const plugin_data *data);

G_END_DECLS

#endif
