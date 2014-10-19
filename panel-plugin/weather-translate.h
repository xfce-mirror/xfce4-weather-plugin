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

#ifndef __WEATHER_TRANSLATE_H__
#define __WEATHER_TRANSLATE_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

const gchar *get_symbol_for_id(gint id);

const gchar *translate_desc(const gchar *desc,
                            gboolean nighttime);

const gchar *translate_moon_phase(const gchar *moon_phase);

/* these return a newly allocated string, that should be freed */
gchar *translate_day(gint weekday);

G_END_DECLS

#endif
