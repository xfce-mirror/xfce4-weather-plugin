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

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "weather-icon.h"



#define DEFAULT_W_THEME "liquid"



GdkPixbuf *
get_icon (const gchar *number,
          gint         size)
{
  GdkPixbuf *image = NULL;
  gchar     *filename;
  gchar     *night;

  if (number == NULL || strlen(number) == 0 || strcmp (number, "-") == 0)
    number = "99";

  filename = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%s" G_DIR_SEPARATOR_S "%s.png",
                              THEMESDIR, DEFAULT_W_THEME, number);

  image = gdk_pixbuf_new_from_file_at_scale (filename, size, size, TRUE, NULL);

  if (G_UNLIKELY (!image)) {
    g_warning ("Unable to open image: %s", filename);
    if (number && strcmp(number, "99")) {
      g_free(filename);
      return get_icon("99", size);
    }
  }
  g_free (filename);

  return image;
}
