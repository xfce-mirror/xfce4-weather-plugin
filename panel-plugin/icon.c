#include <config.h>

#include "icon.h"
#include "debug_print.h"

GtkIconFactory *cfactory = NULL;

void register_icons(gchar *path)
{
        GtkIconSet *iconset;
        int i;
        GdkPixbuf *pixbuf; 
        gchar *filename, *name;

        DEBUG_PRINT("going to register %p\n", cfactory);

        if (cfactory)
                return;

        DEBUG_PRINT("*** %s\n", path);
        

        cfactory = gtk_icon_factory_new();

        for (i = 1; i <= 47; i++) 
        {
                filename = g_strdup_printf("%s%d.png", path, i);
                name = g_strdup_printf("xfceweather_%d", i);
                pixbuf = gdk_pixbuf_new_from_file(filename, NULL);

                if (!pixbuf) 
                {
                       DEBUG_PRINT("Error loading %s\n", filename);
                        continue;
                }
               DEBUG_PRINT("nog een pixbuf %s\n", name);

                iconset = gtk_icon_set_new_from_pixbuf(pixbuf);
                g_object_unref (pixbuf);

                gtk_icon_factory_add(cfactory, name, iconset);

                g_free(filename);
                g_free(name);
        }

        /* and the default icon */
        filename = g_strdup_printf("%s-.png", path);
        pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
        g_free(filename);
        iconset = gtk_icon_set_new_from_pixbuf(pixbuf);
        g_object_unref (pixbuf);

        if (iconset)
                gtk_icon_factory_add(cfactory, "xfceweather_-", iconset);

        gtk_icon_factory_add_default(cfactory);
}

void unregister_icons(void)
{
        /* If there are more weather plugin's loaded, then they can't access
         * the icon's anymore */
/*        gtk_icon_factory_remove_default(cfactory);*/
}


GdkPixbuf *get_icon(GtkWidget *widget, const gchar *number, GtkIconSize size)
{
        GdkPixbuf *image = NULL;
        gchar *str;

        str = g_strdup_printf("xfceweather_%s", number);
        image = gtk_widget_render_icon(widget, str, size, "none");
       DEBUG_PRINT("image %s\n", str);
        g_free(str);

        if (!image)
            g_warning ("weather plugin: No image found");
        
        return image;
}
