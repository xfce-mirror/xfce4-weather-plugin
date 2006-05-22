/* vim: set expandtab ts=8 sw=4: */

/*  This program is free software; you can redistribute it and/or modify
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

#ifndef TRANSLATE_H
#define TRANSLATE_H

G_BEGIN_DECLS

const gchar *
translate_desc          (const gchar *);

const gchar *
translate_bard          (const gchar *);

const gchar *
translate_risk          (const gchar *);

/* these return a newly alocted string, that should be freed */
gchar *
translate_lsup           (const gchar *);

gchar *
translate_day            (const gchar *);

gchar *
translate_wind_direction (const gchar *);

gchar *
translate_wind_speed     (const gchar *, units);

gchar *
translate_time           (const gchar *);

gchar *
translate_visibility     (const gchar *, units);

G_END_DECLS

#endif
