#ifndef SCROLLBOX_H
#define SCROLLBOX_H

#include <gdk/gdk.h>
#include <gtk/gtk.h>

GType gtk_scrollbox_get_type (void);

#define GTK_TYPE_SCROLLBOX             (gtk_scrollbox_get_type())
#define GTK_SCROLLBOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_SCROLLBOX, GtkScrollbox))
#define GTK_SCROLLBOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_SCROLLBOX, GtkScrollboxClass))
#define GTK_IS_SCROLLBOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_SCROLLBOX))
#define GTK_IS_SCROLLBOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_SCROLLBOX))
#define GTK_SCROLLBOX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_SCROLLBOX, GtkScrollboxClass))

typedef struct _GtkScrollbox GtkScrollbox;
typedef struct _GtkScrollboxClass GtkScrollboxClass;

struct _GtkScrollbox {
        GtkDrawingArea parent;

        GPtrArray *labels;
       
        gint draw_callback;
        gint draw_offset;
        gint draw_maxoffset;
        gint draw_middle;
        gint draw_maxmiddle;
        gint draw_timeout;

        GdkPixmap *pixmap;

        
};
struct _GtkScrollboxClass {
        GtkDrawingAreaClass parent;
};
void gtk_scrollbox_set_label(GtkScrollbox *self, gint n, gchar *value);
GtkWidget* gtk_scrollbox_new (void);
void gtk_scrollbox_enablecb (GtkScrollbox *self, gboolean enable);
void gtk_scrollbox_clear (GtkScrollbox *self);
#endif
