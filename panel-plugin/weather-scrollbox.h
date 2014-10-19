/*  Copyright (c) 2003-2014 Xfce Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __WEATHER_SCROLLBOX_H__
#define __WEATHER_SCROLLBOX_H__

G_BEGIN_DECLS

GType gtk_scrollbox_get_type(void);

#define GTK_TYPE_SCROLLBOX \
    (gtk_scrollbox_get_type())
#define GTK_SCROLLBOX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_SCROLLBOX, GtkScrollbox))
#define GTK_SCROLLBOX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_SCROLLBOX, GtkScrollboxClass))
#define GTK_IS_SCROLLBOX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), GTK_TYPE_SCROLLBOX))
#define GTK_IS_SCROLLBOX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), GTK_TYPE_SCROLLBOX))
#define GTK_SCROLLBOX_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), GTK_TYPE_SCROLLBOX, GtkScrollboxClass))


typedef enum {
    FADE_IN,
    FADE_OUT,
    FADE_NONE,
    FADE_SLEEP
} fade_states;

typedef struct _GtkScrollbox GtkScrollbox;
typedef struct _GtkScrollboxClass GtkScrollboxClass;

struct _GtkScrollbox {
    GtkDrawingArea __parent__;

    GList *labels;
    GList *labels_new;
    GList *active;
    guint labels_len;
    guint timeout_id;
    gint offset;
    gboolean animate;
    gboolean visible;
    fade_states fade;
    GtkOrientation orientation;
    gchar *fontname;
    PangoAttrList *pattr_list;
};

struct _GtkScrollboxClass {
    GtkDrawingAreaClass __parent__;
};


void gtk_scrollbox_add_label(GtkScrollbox *self,
                             gint position,
                             const gchar *markup);

void gtk_scrollbox_set_orientation(GtkScrollbox *self,
                                   GtkOrientation orientation);

GtkWidget *gtk_scrollbox_new(void);

void gtk_scrollbox_clear_new(GtkScrollbox *self);

void gtk_scrollbox_swap_labels(GtkScrollbox *self);

void gtk_scrollbox_reset(GtkScrollbox *self);

void gtk_scrollbox_set_animate(GtkScrollbox *self,
                               gboolean animate);

void gtk_scrollbox_set_visible(GtkScrollbox *self,
                               gboolean visible);

void gtk_scrollbox_prev_label(GtkScrollbox *self);

void gtk_scrollbox_next_label(GtkScrollbox *self);

void gtk_scrollbox_set_fontname(GtkScrollbox *self,
                                const gchar *fontname);

void gtk_scrollbox_set_color(GtkScrollbox *self,
                             const GdkColor color);

void gtk_scrollbox_clear_color(GtkScrollbox *self);

G_END_DECLS

#endif
