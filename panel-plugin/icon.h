#ifndef ICON_H
#define ICON_H
#include <gtk/gtk.h>

void register_icons(gchar *path);
GdkPixbuf *get_icon(GtkWidget *widget, const gchar *icon, GtkIconSize size);
void unregister_icons(void);
#endif
