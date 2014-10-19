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

#ifndef __WEATHER_ICON_H__
#define __WEATHER_ICON_H__

G_BEGIN_DECLS

typedef enum {
    SYMBOL_NODATA = 0,
    SYMBOL_SUN,
    SYMBOL_LIGHTCLOUD,
    SYMBOL_PARTLYCLOUD,
    SYMBOL_CLOUD,
    SYMBOL_LIGHTRAINSUN,
    SYMBOL_LIGHTRAINTHUNDERSUN,
    SYMBOL_SLEETSUN,
    SYMBOL_SNOWSUN,
    SYMBOL_LIGHTRAIN,
    SYMBOL_RAIN,
    SYMBOL_RAINTHUNDER,
    SYMBOL_SLEET,
    SYMBOL_SNOW,
    SYMBOL_SNOWTHUNDER,
    SYMBOL_FOG,
    SYMBOL_SUNPOLAR,
    SYMBOL_LIGHTCLOUDPOLAR,
    SYMBOL_LIGHTRAINSUNPOLAR,
    SYMBOL_SNOWSUNPOLAR,
    SYMBOL_SLEETSUNTHUNDER,
    SYMBOL_SNOWSUNTHUNDER,
    SYMBOL_LIGHTRAINTHUNDER,
    SYMBOL_SLEETTHUNDER,
    SYMBOL_COUNT
} symbol_ids;

static const gchar *symbol_names[] = {
    "NODATA",
    "SUN",
    "LIGHTCLOUD",
    "PARTLYCLOUD",
    "CLOUD",
    "LIGHTRAINSUN",
    "LIGHTRAINTHUNDERSUN",
    "SLEETSUN",
    "SNOWSUN",
    "LIGHTRAIN",
    "RAIN",
    "RAINTHUNDER",
    "SLEET",
    "SNOW",
    "SNOWTHUNDER",
    "FOG",
    "SUN",
    "LIGHTCLOUD",
    "LIGHTRAINSUN",
    "SNOWSUN",
    "SLEETSUNTHUNDER",
    "SNOWSUNTHUNDER",
    "LIGHTRAINTHUNDER",
    "SLEETTHUNDER"
};

typedef struct {
    gchar *dir;
    gchar *name;
    gchar *author;
    gchar *description;
    gchar *license;
    GArray *missing_icons;
} icon_theme;


GdkPixbuf *get_icon(const icon_theme *theme,
                    const gchar *icon,
                    gint size,
                    gboolean night);

icon_theme *icon_theme_load_info(const gchar *dir);

icon_theme *icon_theme_load(const gchar *dir);

gchar *get_user_icons_dir(void);

GArray *find_icon_themes(void);

icon_theme *icon_theme_copy(icon_theme *src);

void icon_theme_free(icon_theme *theme);

G_END_DECLS

#endif
