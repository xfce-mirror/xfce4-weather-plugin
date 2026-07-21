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

#define GTK_TYPE_SCROLLBOX (gtk_scrollbox_get_type())
G_DECLARE_FINAL_TYPE (GtkScrollbox, gtk_scrollbox, GTK, SCROLLBOX, GtkDrawingArea)

typedef enum {
    FADE_IN,
    FADE_OUT,
    FADE_NONE,
    FADE_SLEEP
} fade_states;


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
                             const GdkRGBA color);

void gtk_scrollbox_clear_color(GtkScrollbox *self);

G_END_DECLS

#endif
