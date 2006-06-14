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

#include <glib.h>
#include <gtk/gtk.h>

#include "scrollbox.h"
#define LABEL_REFRESH 3000
#define LABEL_SPEED 25

struct label {
    gchar *msg;
    GdkPixmap *pixmap;
};

enum {
    GTK_SCROLLBOX_ENABLECB = 1
};

static gboolean
start_draw_down (GtkScrollbox *self);

static void
start_draw_up   (GtkScrollbox *self);

static void
stop_callback   (GtkScrollbox *self);

static GdkPixmap *
make_pixmap     (GtkScrollbox *, gchar *);

static void
free_label (struct label *lbl)
{
    if (lbl->pixmap)
        g_object_unref (G_OBJECT (lbl->pixmap));
    if (lbl->msg)
        g_free (lbl->msg);
}

static gboolean
draw_up (GtkScrollbox *self)
{
    GdkRectangle update_rect = {0,0, GTK_WIDGET(self)->allocation.width, 
        GTK_WIDGET(self)->allocation.height};
    
    if (self->draw_offset == 0) 
    {
        self->draw_timeout = g_timeout_add(LABEL_REFRESH,
                (GSourceFunc)start_draw_down, self);
        
        return FALSE;
    }
    else
        self->draw_offset++; 
    
    gtk_widget_draw(GTK_WIDGET(self), &update_rect);

    return TRUE;
}

static gboolean
draw_down (GtkScrollbox *self)
{
    GdkRectangle update_rect = {0, 0, GTK_WIDGET(self)->allocation.width, 
        GTK_WIDGET(self)->allocation.height};

    if (self->draw_offset == self->draw_maxoffset) 
    {
        self->draw_timeout = 0;
        start_draw_up(self);

        return FALSE;
    }
    else
        self->draw_offset--;
    
    gtk_widget_draw(GTK_WIDGET(self), &update_rect);

    return TRUE;
}

static void
start_draw_up (GtkScrollbox *self) 
{
    gint          width, height;
    struct label *lbl;
    static gint   i = 0;

    if (self->labels->len == 0)
        return;
    
    if (i >= self->labels->len)
        i = 0;

    lbl = (struct label*)g_ptr_array_index(self->labels, i);
    self->pixmap = lbl->pixmap;

	/* If we failed to create a proper pixmap, try again now */
	if (!lbl->pixmap)
	{
        lbl->pixmap = make_pixmap(self, lbl->msg);
		if (!lbl->pixmap)
		{
			/* Still no pixmap. We need to restart the timer */
			if (self->draw_timeout)
				stop_callback(self);
    		self->draw_timeout = g_timeout_add(LABEL_SPEED, (GSourceFunc)start_draw_up, self);
			return;
		}
	}

    if (self->labels->len == 1) 
    {
        GdkRectangle update_rect = {0, 0, GTK_WIDGET(self)->allocation.width,
            GTK_WIDGET(self)->allocation.height};

        self->pixmap = lbl->pixmap;
        self->draw_offset = 0;
        
        gtk_widget_draw(GTK_WIDGET(self), &update_rect);
        return;
    }
    
    gdk_drawable_get_size(GDK_DRAWABLE(self->pixmap), &width, &height);
    self->draw_middle = self->draw_maxmiddle - width / 2;
    
    self->draw_timeout = g_timeout_add(LABEL_SPEED, (GSourceFunc)draw_up, self);

    i++;
}

static gboolean
start_draw_down (GtkScrollbox *self)
{
    self->draw_timeout = g_timeout_add(LABEL_SPEED, (GSourceFunc)draw_down, self);

    return FALSE;
}

static void
stop_callback (GtkScrollbox *self)
{
    if (self->draw_timeout == 0)
        return;

    g_source_remove(self->draw_timeout);
    self->draw_timeout = 0;
}


static void
start_callback (GtkScrollbox *self)
{
    if (self->draw_timeout)
        stop_callback(self);

    start_draw_up(self);
}

static GdkPixmap *
make_pixmap (GtkScrollbox *self,
             gchar        *value)
{
    GdkWindow      *rootwin;
    PangoLayout    *pl;
    gint            width, height, middle;
    GdkPixmap      *pixmap;
    GtkRequisition  widgsize = {0, }; 
    GtkWidget      *widget = (GtkWidget *)self;
    

    /* If we can't draw yet, don't do anything to avoid screwing things */
    if (!GDK_IS_GC(widget->style->bg_gc[0]))
        return NULL;

    rootwin = gtk_widget_get_root_window(widget);

    pl = gtk_widget_create_pango_layout(widget, NULL);
    pango_layout_set_markup(pl, value, -1);

    pango_layout_get_pixel_size(pl, &width, &height);

    pixmap = gdk_pixmap_new(GDK_DRAWABLE(rootwin), width, height, -1);

    gdk_draw_rectangle(GDK_DRAWABLE(pixmap), 
            widget->style->bg_gc[0],
            TRUE, 0, 0, width, height);

    gdk_draw_layout(GDK_DRAWABLE(pixmap), widget->style->fg_gc[0], 0, 0, pl);

    g_object_unref(G_OBJECT (pl));

    gtk_widget_size_request(widget, &widgsize);

    if (width <= widgsize.width)
        width = widgsize.width;

    if (height <= widgsize.height)
        height = widgsize.height;
    else
        self->draw_maxoffset = -height;

    if (width != widgsize.width || height != widgsize.height)
        gtk_widget_set_size_request(widget, width, height);

    middle = width / 2;
    if (self->draw_maxmiddle < middle)
        self->draw_maxmiddle = middle;

    return pixmap;
}



void
gtk_scrollbox_set_label (GtkScrollbox *self,
                         gint          n,
                         gchar        *value)
{
    gboolean      append = TRUE;
    GdkPixmap    *newpixmap;
    struct label *newlbl;
    
    if (n != -1)
        append = FALSE;
    
    if (!append)
    {
        struct label *lbl = (struct label*)g_ptr_array_index(self->labels, n);

        if (lbl) 
            free_label(lbl);

        newlbl = lbl;
    }
    else
    {
        newlbl = g_new0(struct label, 1);
        g_ptr_array_add(self->labels, newlbl);
    }

    newpixmap = make_pixmap(self, value);

    newlbl->pixmap = newpixmap;
    newlbl->msg = g_strdup(value);
}

static void
gtk_scrollbox_set_property (GObject      *object,
                            guint         property_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
    GtkScrollbox *self = (GtkScrollbox *)object;

    switch (property_id)
    {
        case GTK_SCROLLBOX_ENABLECB:
        {
            gboolean realvalue = g_value_get_boolean(value);
            
            if (!realvalue && self->draw_timeout)
                stop_callback(self);
            else if (realvalue && !self->draw_timeout) 
                start_callback(self);
                  
            break;
        }
        default:
            /* We don't have any other property... */
            g_assert (FALSE);
            
            break;
    }
}

static void
gtk_scrollbox_get_property (GObject      *object,
                            guint         property_id,
                            GValue       *value,
                            GParamSpec   *pspec)
{
    g_assert(FALSE);
}

static void
gtk_scrollbox_finalize (GObject *gobject)
{
    GtkScrollbox *self = (GtkScrollbox *)gobject;
    guint i;

    if (self->draw_timeout) {
        g_source_remove(self->draw_timeout);
        self->draw_timeout = 0;
    }
    
    if (self->labels->len > 0)
    {
        for (i = 0; i < self->labels->len; i++)
        {
            struct label *lbl = (struct label*)g_ptr_array_index(self->labels, i); 

            g_object_unref (G_OBJECT (lbl->pixmap));
            g_free(lbl->msg);
        }
        g_ptr_array_free(self->labels, TRUE);
    }
}

static void
redraw_labels (GtkWidget *widget,
               GtkStyle  *previous_style)
{
    GtkScrollbox *self = GTK_SCROLLBOX(widget);
    guint i;
    
    if (self->labels->len < 1)
        return; 

    stop_callback(self);

    gtk_widget_set_size_request(GTK_WIDGET(self), 0, 0);
    self->draw_middle = 0;
    self->draw_maxmiddle = 0;

    for (i = 0; i < self->labels->len; i++)
    {
        GdkPixmap *newpixmap;
        struct label *lbl = (struct label*)g_ptr_array_index(self->labels, i);

        if (!lbl->msg)
            continue;

        newpixmap = make_pixmap(self, lbl->msg);

        if (lbl->pixmap)
            g_object_unref (G_OBJECT (lbl->pixmap));

        lbl->pixmap = newpixmap;
    }

    start_callback(self);

}
            

static void
gtk_scrollbox_instance_init (GTypeInstance *instance,
                             gpointer       g_class)
{
      GtkScrollbox *self = (GtkScrollbox *)instance;

/*      GTK_WIDGET_SET_FLAGS (GTK_WIDGET(self), GTK_NO_WINDOW);*/

      self->draw_timeout = 0;
      self->labels = g_ptr_array_new();
      self->pixmap = NULL;

      g_signal_connect(self, "style-set", G_CALLBACK(redraw_labels), NULL);
}

static gboolean
gtk_scrollbox_expose (GtkWidget      *widget,
                      GdkEventExpose *event) 
{
    GtkScrollbox *self = (GtkScrollbox *)widget;

    if (self->pixmap)
        gdk_draw_drawable(widget->window,
            widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
            self->pixmap,
            0, self->draw_offset,
            self->draw_middle, 0,
            widget->allocation.width,widget->allocation.height);

    return FALSE;
}

void
gtk_scrollbox_enablecb (GtkScrollbox *self,
                        gboolean      enable)
{
    GValue val = {0,};

    g_value_init (&val, G_TYPE_BOOLEAN);
    g_value_set_boolean (&val, enable);

    g_object_set_property(G_OBJECT(self), "enablecb", &val);
}

GtkWidget *
gtk_scrollbox_new (void) 
{
    return GTK_WIDGET(g_object_new (GTK_TYPE_SCROLLBOX, NULL));
}
    
static void
gtk_scrollbox_class_init (gpointer g_class,
                          gpointer g_class_data)
{
  
  GObjectClass *gobject_class = G_OBJECT_CLASS(g_class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(g_class);
  
  GParamSpec *scrollbox_param_spec;

  gobject_class->set_property = gtk_scrollbox_set_property;
  gobject_class->get_property = gtk_scrollbox_get_property;
  
  scrollbox_param_spec = g_param_spec_boolean("enablecb",
          "Enable callback",
          "Enable or disable the callback",
          FALSE,
          G_PARAM_READWRITE);
  g_object_class_install_property (gobject_class,
       GTK_SCROLLBOX_ENABLECB,
       scrollbox_param_spec);

  widget_class->expose_event = gtk_scrollbox_expose;
  gobject_class->finalize = gtk_scrollbox_finalize;
}


GType
gtk_scrollbox_get_type (void)
{
    static GType type = 0;

    if (type == 0) {

        static const GTypeInfo info = {
            sizeof (GtkScrollboxClass),
            NULL,   /* base_init */
            NULL,   /* base_finalize */
            gtk_scrollbox_class_init,   /* class_init */
            NULL,   /* class_finalize */
            NULL,   /* class_data */
            sizeof (GtkScrollbox),
            0,      /* n_preallocs */
            gtk_scrollbox_instance_init    /* instance_init */
        };

        type = g_type_register_static (GTK_TYPE_DRAWING_AREA,
                "GtkScrollbox", &info, 0);
    }

    return type;
}

void
gtk_scrollbox_clear (GtkScrollbox *self)
{
    stop_callback(self);

    while(self->labels->len > 0)
    { 
        struct label *lbl = (struct label*) g_ptr_array_index(self->labels, 0);
        free_label(lbl);

        g_ptr_array_remove_index (self->labels, 0);
    }
    
    self->pixmap = NULL;
    gtk_widget_set_size_request(GTK_WIDGET(self), 0, 0);
    self->draw_middle = 0;
    self->draw_maxmiddle = 0;
}
