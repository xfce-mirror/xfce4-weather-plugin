#include "icon.h"
#include "debug_print.h"

GtkIconFactory *cfactory = NULL;

void register_icons(gchar *path)
{
        GtkIconSet *iconset;
        int i;
        GdkPixbuf *pixbuf;
        GtkIconFactory *factory;
        gchar *filename, *name;

       DEBUG_PRINT("going to register %p\n", cfactory);

        if (cfactory)
                return;

       DEBUG_PRINT("*** %s\n", path);
        

        cfactory = gtk_icon_factory_new();

        for (i = 1; i <= 44; i++) 
        {
                filename = g_strdup_printf("%s%d.png", path, i);
                name = g_strdup_printf("%s%d", "xfceweather_", i);
                pixbuf = gdk_pixbuf_new_from_file(filename, NULL);

                if (!pixbuf) 
                {
                       DEBUG_PRINT("Error loading %s\n", filename);
                        continue;
                }
               DEBUG_PRINT("nog een pixbuf %s\n", name);

                iconset = gtk_icon_set_new_from_pixbuf(pixbuf);

                gtk_icon_factory_add(cfactory, name, iconset);

                g_free(filename);
                g_free(name);
        }

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

        return image;
}
