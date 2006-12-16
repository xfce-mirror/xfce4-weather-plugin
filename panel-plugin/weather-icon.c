/*  $Id$
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

#include "weather-icon.h"

#define DEFAULT_W_THEME "liquid"

GdkPixbuf *
get_icon (const gchar *number,
          GtkIconSize  size)
{
    GdkPixbuf *image = NULL;
    gchar     *filename;
    gint       width, height;

    gtk_icon_size_lookup (size, &width, &height);

    filename = g_strdup_printf ("%s%s%s%s%s.png",
            THEMESDIR, G_DIR_SEPARATOR_S,
            DEFAULT_W_THEME, G_DIR_SEPARATOR_S,
            number);

    image = gdk_pixbuf_new_from_file_at_scale (filename,
	    width, height,
            TRUE, NULL);

    g_free (filename);

    if (!image)
        g_warning ("Weather Plugin: No image found");

    return image;
}
