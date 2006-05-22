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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include "icon.h"

GtkIconFactory *cfactory = NULL;

void
register_icons (gchar *path)
{
    GtkIconSet *iconset;
    guint       i;
    GdkPixbuf  *pixbuf; 
    gchar      *filename, *name;

    if (cfactory)
        return;

    cfactory = gtk_icon_factory_new ();

    for (i = 1; i <= 47; i++) 
    {
        filename = g_strdup_printf ("%s%d.png", path, i);
        name = g_strdup_printf ("xfceweather_%d", i);
        pixbuf = gdk_pixbuf_new_from_file (filename, NULL);

        if (!pixbuf) 
            continue;

        iconset = gtk_icon_set_new_from_pixbuf (pixbuf);
        g_object_unref (pixbuf);

        gtk_icon_factory_add (cfactory, name, iconset);

        g_free (filename);
        g_free (name);
    }

    /* and the default icon */
    filename = g_strdup_printf ("%s-.png", path);
    pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
    g_free (filename);
    iconset = gtk_icon_set_new_from_pixbuf (pixbuf);
    g_object_unref (pixbuf);

    if (iconset)
        gtk_icon_factory_add (cfactory, "xfceweather_-", iconset);

    gtk_icon_factory_add_default (cfactory);
}

GdkPixbuf *
get_icon (GtkWidget   *widget,
           const gchar *number,
           GtkIconSize  size)
{
    GdkPixbuf *image = NULL;
    gchar     *str;

    str = g_strdup_printf ("xfceweather_%s", number);
    image = gtk_widget_render_icon (widget, str, size, "none");
    
    g_free (str);

    if (!image)
        g_warning ("weather plugin: No image found");
    
    return image;
}
