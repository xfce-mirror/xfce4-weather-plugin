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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stddef.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4panel/libxfce4panel.h>

#include "weather-scrollbox.h"

#define LABEL_SLEEP (3)       /* sleep time in seconds */
#define LABEL_SLEEP_LONG (6)  /* sleep time in seconds for FADE_NONE */
#define LABEL_SPEED (25)      /* animation speed, delay in ms */


static void gtk_scrollbox_finalize(GObject *object);

static void gtk_scrollbox_size_request(GtkWidget *widget,
                                       GtkRequisition *requisition);

static gboolean gtk_scrollbox_expose_event(GtkWidget *widget,
                                           GdkEventExpose *event);

static gboolean gtk_scrollbox_control_loop(gpointer user_data);

G_DEFINE_TYPE(GtkScrollbox, gtk_scrollbox, GTK_TYPE_DRAWING_AREA)


static void
gtk_scrollbox_class_init(GtkScrollboxClass *klass)
{
    GObjectClass *gobject_class;
    GtkWidgetClass *widget_class;

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = gtk_scrollbox_finalize;

    widget_class = GTK_WIDGET_CLASS(klass);
    widget_class->size_request = gtk_scrollbox_size_request;
    widget_class->expose_event = gtk_scrollbox_expose_event;
}


static void
gtk_scrollbox_init(GtkScrollbox *self)
{
    GTK_WIDGET_SET_FLAGS(self, GTK_NO_WINDOW);

    self->labels = NULL;
    self->labels_new = NULL;
    self->timeout_id = 0;
    self->labels_len = 0;
    self->offset = 0;
    self->fade = FADE_OUT;
    self->active = NULL;
    self->animate = FALSE;
    self->visible = FALSE;
    self->orientation = GTK_ORIENTATION_HORIZONTAL;
    self->fontname = NULL;
    self->pattr_list = pango_attr_list_new();
}


static void
gtk_scrollbox_labels_free(GtkScrollbox *self)
{
    /* free all the labels */
    g_list_foreach(self->labels, (GFunc) g_object_unref, NULL);
    g_list_free(self->labels);
    self->labels = NULL;
}


static void
gtk_scrollbox_finalize(GObject *object)
{
    GtkScrollbox *self = GTK_SCROLLBOX(object);

    /* stop running timeout */
    if (self->timeout_id != 0)
        g_source_remove(self->timeout_id);

    /* free all the labels */
    gtk_scrollbox_labels_free(self);

    /* free all the new labels */
    gtk_scrollbox_clear_new(self);

    /* free everything else */
    g_free(self->fontname);
    pango_attr_list_unref(self->pattr_list);

    G_OBJECT_CLASS(gtk_scrollbox_parent_class)->finalize(object);
}


static void
gtk_scrollbox_set_font(GtkScrollbox *self,
                       PangoLayout *layout)
{
    GList *li;
    PangoFontDescription *desc = NULL;

    if (self->fontname)
        desc = pango_font_description_from_string(self->fontname);

    if (layout) {
        pango_layout_set_font_description(layout, desc);
        pango_layout_set_attributes(layout, self->pattr_list);
        pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
    } else
        for (li = self->labels; li != NULL; li = li->next) {
            layout = PANGO_LAYOUT(li->data);
            pango_layout_set_font_description(layout, desc);
            pango_layout_set_attributes(layout, self->pattr_list);
            pango_layout_set_alignment(layout, PANGO_ALIGN_CENTER);
        }
    pango_font_description_free(desc);
}


static void
gtk_scrollbox_size_request(GtkWidget *widget,
                           GtkRequisition *requisition)
{
    GtkScrollbox *self = GTK_SCROLLBOX(widget);
    GList *li;
    PangoLayout *layout;
    PangoRectangle logical_rect;
    gint width, height;

    requisition->width = 0;
    requisition->height = 0;

    for (li = self->labels; li != NULL; li = li->next) {
        layout = PANGO_LAYOUT(li->data);
        pango_layout_get_extents(layout, NULL, &logical_rect);

        if (self->orientation == GTK_ORIENTATION_HORIZONTAL) {
            width = PANGO_PIXELS(logical_rect.width);
            height = PANGO_PIXELS(logical_rect.height);
        } else {
            height = PANGO_PIXELS(logical_rect.width);
            width = PANGO_PIXELS(logical_rect.height);
        }

        requisition->width = MAX(width, requisition->width);
        requisition->height = MAX(height, requisition->height);
    }
}


static gboolean
gtk_scrollbox_expose_event(GtkWidget *widget,
                           GdkEventExpose *event)
{
    GtkScrollbox *self = GTK_SCROLLBOX(widget);
    PangoLayout *layout;
    gint width, height;
    PangoRectangle logical_rect;
    gboolean result = FALSE;
    PangoMatrix matrix = PANGO_MATRIX_INIT;

    if (GTK_WIDGET_CLASS(gtk_scrollbox_parent_class)->expose_event != NULL)
        result = GTK_WIDGET_CLASS
            (gtk_scrollbox_parent_class)->expose_event(widget, event);

    if (self->active != NULL) {
        layout = PANGO_LAYOUT(self->active->data);
        pango_matrix_rotate(&matrix,
                            (self->orientation == GTK_ORIENTATION_HORIZONTAL)
                            ? 0.0 : -90.0);
        pango_context_set_matrix(pango_layout_get_context(layout), &matrix);
        pango_layout_get_extents(layout, NULL, &logical_rect);

        if (self->orientation == GTK_ORIENTATION_HORIZONTAL) {
            width = widget->allocation.x
                + (widget->allocation.width
                   - PANGO_PIXELS(logical_rect.width)) / 2;
            height = widget->allocation.y
                + (widget->allocation.height
                   - PANGO_PIXELS(logical_rect.height)) / 2
                + (self->fade == FADE_IN || self->fade == FADE_OUT
                   ? self->offset : 0);
        } else {
            width = widget->allocation.x
                + (widget->allocation.width
                   - PANGO_PIXELS(logical_rect.height)) / 2
                + (self->fade == FADE_IN || self->fade == FADE_OUT
                   ? self->offset : 0);
            height = widget->allocation.y
                + (widget->allocation.height
                   - PANGO_PIXELS(logical_rect.width)) / 2;
        }

        gtk_paint_layout(widget->style,
                         widget->window,
                         GTK_WIDGET_STATE(widget), TRUE,
                         &event->area, widget,
                         "GtkScrollbox", width, height, layout);
    }
    return result;
}


void
gtk_scrollbox_add_label(GtkScrollbox *self,
                        const gint position,
                        const gchar *markup)
{
    PangoLayout *layout;

    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    layout = gtk_widget_create_pango_layout(GTK_WIDGET(self), NULL);
    pango_layout_set_markup(layout, markup, -1);
    gtk_scrollbox_set_font(self, layout);
    self->labels_new = g_list_insert(self->labels_new, layout, position);
}


/*
 * Sets new labels active if there are any, or advances the current
 * label.
 */
void
gtk_scrollbox_swap_labels(GtkScrollbox *self)
{
    gint pos;

    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    /* No new label, so simply switch to the next */
    if (G_LIKELY(self->labels_new == NULL)) {
        gtk_scrollbox_next_label(self);
        return;
    }

    /* Keep current list position if possible */
    if (self->active && self->labels_len > 1)
        pos = g_list_position(self->labels, self->active);
    else
        pos = -1;

    self->labels_len = g_list_length(self->labels_new);
    if (pos >= (gint) self->labels_len)
        pos = -1;
    self->active = g_list_nth(self->labels_new, pos + 1);
    if (self->active == NULL)
        self->active = self->labels_new;

    /* clear the existing labels */
    gtk_scrollbox_labels_free(self);
    self->labels = self->labels_new;
    self->labels_new = NULL;

    gtk_widget_queue_resize(GTK_WIDGET(self));
}


static gboolean
gtk_scrollbox_fade_in(gpointer user_data)
{
    GtkScrollbox *self = GTK_SCROLLBOX(user_data);

    /* decrease counter */
    if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        self->offset--;
    else
        self->offset++;

    gtk_widget_queue_draw(GTK_WIDGET(self));

    if ((self->orientation == GTK_ORIENTATION_HORIZONTAL && self->offset > 0) ||
        (self->orientation == GTK_ORIENTATION_VERTICAL && self->offset < 0))
        return TRUE;

    (void) gtk_scrollbox_control_loop(self);
    return FALSE;
}


static gboolean
gtk_scrollbox_fade_out(gpointer user_data)
{
    GtkScrollbox *self = GTK_SCROLLBOX(user_data);

    /* increase counter */
    if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
        self->offset++;
    else
        self->offset--;

    gtk_widget_queue_draw(GTK_WIDGET(self));

    if ((self->orientation == GTK_ORIENTATION_HORIZONTAL &&
         self->offset < GTK_WIDGET(self)->allocation.height) ||
        (self->orientation == GTK_ORIENTATION_VERTICAL &&
         self->offset > 0 - GTK_WIDGET(self)->allocation.width))
        return TRUE;

    (void) gtk_scrollbox_control_loop(self);
    return FALSE;
}


/*
 * This control loop is called between any animation steps except
 * sleeps and determines what to do next.
 */
static gboolean
gtk_scrollbox_control_loop(gpointer user_data)
{
    GtkScrollbox *self = GTK_SCROLLBOX(user_data);

    if (self->timeout_id != 0) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }

    /* determine what to do next */
    switch(self->fade) {
    case FADE_IN:
        self->fade = FADE_SLEEP;
        break;
    case FADE_OUT:
        if (self->animate)
            self->fade = FADE_IN;
        else
            self->fade = FADE_NONE;
        gtk_scrollbox_swap_labels(self);
        break;
    case FADE_NONE:
        if (self->animate && self->labels_len > 1)
            self->fade = FADE_OUT;
        else {
            self->fade = FADE_NONE;
            gtk_scrollbox_swap_labels(self);
        }
        break;
    case FADE_SLEEP:
        if (self->animate && self->labels_len > 1)
            self->fade = FADE_OUT;
        else
            self->fade = FADE_NONE;
        break;
    }

    /* now perform the next action */
    switch(self->fade) {
    case FADE_IN:
        if (self->labels_len > 1) {
            if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
                self->offset = GTK_WIDGET(self)->allocation.height;
            else
                self->offset = 0 - GTK_WIDGET(self)->allocation.width;
        } else
            self->offset = 0;
        self->timeout_id = g_timeout_add(LABEL_SPEED,
                                         gtk_scrollbox_fade_in,
                                         self);
        break;
    case FADE_OUT:
        self->offset = 0;
        self->timeout_id = g_timeout_add(LABEL_SPEED,
                                         gtk_scrollbox_fade_out,
                                         self);
        break;
    case FADE_SLEEP:
        self->timeout_id = g_timeout_add_seconds(LABEL_SLEEP,
                                                 gtk_scrollbox_control_loop,
                                                 self);
        break;
    case FADE_NONE:
        if (self->orientation == GTK_ORIENTATION_HORIZONTAL)
            self->offset = GTK_WIDGET(self)->allocation.height;
        else
            self->offset = GTK_WIDGET(self)->allocation.width;
        self->timeout_id = g_timeout_add_seconds(LABEL_SLEEP_LONG,
                                                 gtk_scrollbox_control_loop,
                                                 self);
        break;
    }

    gtk_widget_queue_resize(GTK_WIDGET(self));
    return FALSE;
}


void
gtk_scrollbox_set_orientation(GtkScrollbox *self,
                              const GtkOrientation orientation)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    self->orientation = orientation;
    gtk_widget_queue_resize(GTK_WIDGET(self));
}


GtkWidget *
gtk_scrollbox_new(void)
{
    return g_object_new(GTK_TYPE_SCROLLBOX, NULL);
}


void
gtk_scrollbox_clear_new(GtkScrollbox *self)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    /* free all the new labels */
    g_list_foreach(self->labels_new, (GFunc) g_object_unref, NULL);
    g_list_free(self->labels_new);
    self->labels_new = NULL;
}


void
gtk_scrollbox_reset(GtkScrollbox *self)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    if (self->timeout_id != 0) {
        g_source_remove(self->timeout_id);
        self->timeout_id = 0;
    }
    self->fade = FADE_OUT;
    gtk_scrollbox_prev_label(self);
    (void) gtk_scrollbox_control_loop(self);
}


void
gtk_scrollbox_set_animate(GtkScrollbox *self,
                          const gboolean animate)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    self->animate = animate;
}


void
gtk_scrollbox_set_visible(GtkScrollbox *self,
                          const gboolean visible)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    gtk_widget_set_visible(GTK_WIDGET(self), visible);
    self->visible = visible;
    if (visible) {
        if (self->timeout_id == 0) {
            self->fade = FADE_NONE;
            (void) gtk_scrollbox_control_loop(self);
        } else {
            /* update immediately if there's only one or no label,
               typically this is the case at startup */
            if (self->active == NULL || self->labels_len <= 1)
                (void) gtk_scrollbox_control_loop(self);
        }
    } else
        if (self->timeout_id != 0) {
            g_source_remove(self->timeout_id);
            self->timeout_id = 0;
        }
}


void
gtk_scrollbox_prev_label(GtkScrollbox *self)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    if (self->labels_len > 1) {
        if (self->active->prev != NULL)
            self->active = self->active->prev;
        else
            self->active = g_list_last(self->labels);
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}


void
gtk_scrollbox_next_label(GtkScrollbox *self)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    if (self->labels_len > 1) {
        if (self->active->next != NULL)
            self->active = self->active->next;
        else
            self->active = self->labels;
        gtk_widget_queue_draw(GTK_WIDGET(self));
    }
}


void
gtk_scrollbox_set_fontname(GtkScrollbox *self,
                           const gchar *fontname)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    g_free(self->fontname);
    self->fontname = g_strdup(fontname);

    /* update all labels */
    gtk_scrollbox_set_font(self, NULL);
    gtk_widget_queue_resize(GTK_WIDGET(self));
}


void
gtk_scrollbox_set_color(GtkScrollbox *self,
                        const GdkColor color)
{
    PangoAttribute *pattr;

    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    pattr = pango_attr_foreground_new(color.red,
                                      color.green,
                                      color.blue);
    pango_attr_list_change(self->pattr_list, pattr);

    /* update all labels */
    gtk_scrollbox_set_font(self, NULL);
    gtk_widget_queue_resize(GTK_WIDGET(self));
}


void
gtk_scrollbox_clear_color(GtkScrollbox *self)
{
    g_return_if_fail(GTK_IS_SCROLLBOX(self));

    pango_attr_list_unref(self->pattr_list);
    self->pattr_list = pango_attr_list_new();

    /* update all labels */
    gtk_scrollbox_set_font(self, NULL);
    gtk_widget_queue_resize(GTK_WIDGET(self));
}
