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

#include <stddef.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

#include "weather-scrollbox.h"
#define LABEL_REFRESH 3000
#define LABEL_SPEED   25

static void gtk_scrollbox_finalize (GObject *object);
static void gtk_scrollbox_size_request (GtkWidget *widget, GtkRequisition *requisition);
static gboolean gtk_scrollbox_expose_event (GtkWidget *widget, GdkEventExpose *event);
static gboolean gtk_scrollbox_fade_out (gpointer user_data);

G_DEFINE_TYPE (GtkScrollbox, gtk_scrollbox, GTK_TYPE_DRAWING_AREA)

static void
gtk_scrollbox_class_init (GtkScrollboxClass *klass)
{
  GObjectClass   *gobject_class;
  GtkWidgetClass *widget_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = gtk_scrollbox_finalize;

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->size_request = gtk_scrollbox_size_request;
  widget_class->expose_event = gtk_scrollbox_expose_event;
  //widget_class->style_changed = gtk_scrollbox_style_changed;
}



static void
gtk_scrollbox_init (GtkScrollbox *self)
{
  GTK_WIDGET_SET_FLAGS (self, GTK_NO_WINDOW);

  self->labels = NULL;
  self->timeout_id = 0;
  self->offset = 0;
  self->active = NULL;
}



static void
gtk_scrollbox_finalize (GObject *object)
{
  GtkScrollbox *self = GTK_SCROLLBOX (object);

  /* stop running timeout */
  if (self->timeout_id != 0)
    g_source_remove (self->timeout_id);

  /* free all the labels */
  g_slist_foreach (self->labels, (GFunc) g_object_unref, NULL);
  g_slist_free (self->labels);

  G_OBJECT_CLASS (gtk_scrollbox_parent_class)->finalize (object);
}



static void
gtk_scrollbox_size_request (GtkWidget      *widget,
                            GtkRequisition *requisition)
{
  GtkScrollbox   *self = GTK_SCROLLBOX (widget);
  GSList         *li;
  PangoLayout    *layout;
  PangoRectangle  logical_rect;
  gint            width, height;

  requisition->width = 0;
  requisition->height = 0;

  for (li = self->labels; li != NULL; li = li->next)
    {
      layout = PANGO_LAYOUT (li->data);
      pango_layout_get_extents (layout, NULL, &logical_rect);
      width = PANGO_PIXELS (logical_rect.width);
      height = PANGO_PIXELS (logical_rect.height);

      requisition->width = MAX (width, requisition->width);
      requisition->height = MAX (height, requisition->height);
    }
}


static gboolean
gtk_scrollbox_expose_event (GtkWidget      *widget,
                            GdkEventExpose *event)
{
  GtkScrollbox   *self =  GTK_SCROLLBOX (widget);
  PangoLayout    *layout;
  gint            width, height;
  PangoRectangle  logical_rect;
  gboolean        result = FALSE;

  if (GTK_WIDGET_CLASS (gtk_scrollbox_parent_class)->expose_event != NULL)
    result = GTK_WIDGET_CLASS (gtk_scrollbox_parent_class)->expose_event (widget, event);

  if (self->active != NULL)
    {
      layout = PANGO_LAYOUT (self->active->data);
      pango_layout_get_extents (layout, NULL, &logical_rect);
      width = PANGO_PIXELS (logical_rect.width);
      height = PANGO_PIXELS (logical_rect.height);

      gtk_paint_layout (widget->style,
                        widget->window,
                        GTK_WIDGET_STATE (widget),
                        TRUE, &event->area,
                        widget, "GtkScrollbox",
                        widget->allocation.x + (widget->allocation.width - width) / 2,
                        widget->allocation.y + (widget->allocation.height - height) / 2 + (self->animate ? self->offset : 0),
                        layout);

    }

  return result;
}



static gboolean
gtk_scrollbox_sleep (gpointer user_data)
{
  GtkScrollbox *self = GTK_SCROLLBOX (user_data);

  self->timeout_id = g_timeout_add (LABEL_SPEED, gtk_scrollbox_fade_out, self);

  return FALSE;
}



static gboolean
gtk_scrollbox_fade_in (gpointer user_data)
{
  GtkScrollbox *self = GTK_SCROLLBOX (user_data);

  /* decrease counter */
  self->offset--;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  if (self->offset > 0)
    return TRUE;

  self->timeout_id = g_timeout_add (LABEL_REFRESH, gtk_scrollbox_sleep, self);

  return FALSE;
}



static gboolean
gtk_scrollbox_fade_out (gpointer user_data)
{
  GtkScrollbox *self = GTK_SCROLLBOX (user_data);

  /* increase counter */
  self->offset++;

  gtk_widget_queue_draw (GTK_WIDGET (self));

  if (self->offset < GTK_WIDGET (self)->allocation.height)
    return TRUE;

  if (self->active != NULL)
    {
      if (self->active->next != NULL)
        self->active = self->active->next;
      else
        self->active = self->labels;

      self->timeout_id = g_timeout_add (LABEL_SPEED,
                                        gtk_scrollbox_fade_in, self);
    }

  return FALSE;
}



static void
gtk_scrollbox_start_fade (GtkScrollbox *self)
{
  if (self->timeout_id != 0)
    {
      g_source_remove (self->timeout_id);
      self->timeout_id = 0;
    }

  self->active = self->labels;

  if (g_slist_length (self->labels) > 1)
    {
      self->offset = GTK_WIDGET (self)->allocation.height;
      self->timeout_id = g_timeout_add (25, gtk_scrollbox_fade_in,
                                        self);
    }
  else
    {
      self->offset = 0;
    }
}



void
gtk_scrollbox_set_label (GtkScrollbox *self,
                         gint          position,
                         gchar        *markup)
{
  PangoLayout *layout;


  g_return_if_fail (GTK_IS_SCROLLBOX (self));

  layout = gtk_widget_create_pango_layout (GTK_WIDGET (self), NULL);
  pango_layout_set_markup (layout, markup, -1);
  self->labels = g_slist_insert (self->labels, layout, position);
  gtk_widget_queue_resize (GTK_WIDGET (self));

  gtk_scrollbox_start_fade (self);
}



GtkWidget *
gtk_scrollbox_new (void)
{
  return g_object_new (GTK_TYPE_SCROLLBOX, NULL);
}



void
gtk_scrollbox_clear (GtkScrollbox *self)
{
  g_return_if_fail (GTK_IS_SCROLLBOX (self));

  g_slist_foreach (self->labels, (GFunc) g_object_unref, NULL);
  g_slist_free (self->labels);
  self->labels = NULL;

  gtk_widget_queue_resize (GTK_WIDGET (self));
}



void gtk_scrollbox_set_animate(GtkScrollbox *self, gboolean animate)
{
  self->animate = animate;
}



void gtk_scrollbox_next_label(GtkScrollbox *self)
{
  if (self->active->next != NULL)
    {
      self->active = self->active->next;

      gtk_widget_queue_resize (GTK_WIDGET (self));
    }
}
