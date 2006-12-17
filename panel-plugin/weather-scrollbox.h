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

#ifndef __WEATHER_SCROLLBOX_H__
#define __WEATHER_SCROLLBOX_H__

G_BEGIN_DECLS

GType gtk_scrollbox_get_type (void);

#define GTK_TYPE_SCROLLBOX             (gtk_scrollbox_get_type())
#define GTK_SCROLLBOX(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_SCROLLBOX, GtkScrollbox))
#define GTK_SCROLLBOX_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_SCROLLBOX, GtkScrollboxClass))
#define GTK_IS_SCROLLBOX(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_SCROLLBOX))
#define GTK_IS_SCROLLBOX_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_SCROLLBOX))
#define GTK_SCROLLBOX_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_SCROLLBOX, GtkScrollboxClass))

typedef struct _GtkScrollbox GtkScrollbox;
typedef struct _GtkScrollboxClass GtkScrollboxClass;

struct _GtkScrollbox
{
  GtkDrawingArea  parent;

  GPtrArray      *labels;

  gint            draw_offset;
  gint            draw_maxoffset;
  gint            draw_middle;
  gint            draw_maxmiddle;
  gint            draw_timeout;

  GdkPixmap      *pixmap;
};

struct _GtkScrollboxClass
{
  GtkDrawingAreaClass parent;
};

void gtk_scrollbox_set_label (GtkScrollbox * self, gint n, gchar * value);

GtkWidget *gtk_scrollbox_new (void);

void gtk_scrollbox_enablecb (GtkScrollbox * self, gboolean enable);

void gtk_scrollbox_clear (GtkScrollbox * self);

G_END_DECLS

#endif
