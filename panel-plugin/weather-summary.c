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

#include <libxfce4ui/libxfce4ui.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather-http.h"
#include "weather.h"

#include "weather-summary.h"
#include "weather-translate.h"
#include "weather-icon.h"


static gboolean lnk_clicked (GtkTextTag *tag, GObject *obj,
			     GdkEvent *event, GtkTextIter *iter,
			     GtkWidget *textview);

#define BORDER                           8
#define APPEND_BTEXT(text)               gtk_text_buffer_insert_with_tags(GTK_TEXT_BUFFER(buffer),\
                                                                          &iter, text, -1, btag, NULL);
#define APPEND_TEXT_ITEM_REAL(text)      gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer), \
                                                                &iter, text, -1);\
                                         g_free (value);
#define APPEND_TEXT_ITEM(text, item)     value = g_strdup_printf("\t%s%s%s %s\n",\
                                                                 text, text?": ":"", \
                                                                 get_data(data->weatherdata, item),\
                                                                 get_unit(data->weatherdata, data->unit, item));\
                                         APPEND_TEXT_ITEM_REAL(value);
#define APPEND_LINK_ITEM(prefix, text, url, lnk_tag) \
					 gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer), \
                                                                &iter, prefix, -1);\
					 gtk_text_buffer_insert_with_tags(GTK_TEXT_BUFFER(buffer), \
                                                                &iter, text, -1, lnk_tag, NULL);\
					 gtk_text_buffer_insert(GTK_TEXT_BUFFER(buffer), \
                                                                &iter, "\n", -1);\
					 g_object_set_data_full(G_OBJECT(lnk_tag), "url", g_strdup(url), g_free); \
					 g_signal_connect(G_OBJECT(lnk_tag), "event", \
						G_CALLBACK(lnk_clicked), NULL);



static GtkTooltips *tooltips = NULL;

static gboolean lnk_clicked (GtkTextTag *tag, GObject *obj,
			     GdkEvent *event, GtkTextIter *iter,
			     GtkWidget *textview)
{
  if (event->type == GDK_BUTTON_RELEASE) {
    const gchar *url = g_object_get_data(G_OBJECT(tag), "url");
    gchar *str = g_strdup_printf("exo-open --launch WebBrowser %s", url);

    g_spawn_command_line_async(str, NULL);
    g_free(str);
  } else if (event->type == GDK_LEAVE_NOTIFY) {
     gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(obj),
				GTK_TEXT_WINDOW_TEXT), NULL);
  }
  return FALSE;
}

static gboolean
icon_clicked (GtkWidget      *widget,
              GdkEventButton *event,
              gpointer        user_data)
{
  return lnk_clicked(user_data, NULL, (GdkEvent *)(event), NULL, NULL);
}

static GdkCursor *hand_cursor = NULL;
static GdkCursor *text_cursor = NULL;
static gboolean on_icon = FALSE;
static gboolean view_motion_notify(GtkWidget *widget,
				   GdkEventMotion *event,
				   GtkWidget *view)
{
  GtkTextBuffer *buffer;

  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));

  if (event->x != -1 && event->y != -1) {
    gint bx, by;
    GtkTextIter iter;
    GSList *tags;
    GSList *cur;

    gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(view),
    						GTK_TEXT_WINDOW_WIDGET,
    						event->x, event->y, &bx, &by);
    gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(view),
    						&iter, bx, by);
    tags = gtk_text_iter_get_tags(&iter);
    for (cur = tags; cur != NULL; cur = cur->next) {
      GtkTextTag *tag = cur->data;
      if (g_object_get_data(G_OBJECT(tag), "url")) {
        gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(view),
				   GTK_TEXT_WINDOW_TEXT), hand_cursor);
        return FALSE;
      }
    }
  }
  if (!on_icon)
    gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(view),
			     GTK_TEXT_WINDOW_TEXT), text_cursor);
  return FALSE;
}

static gboolean icon_motion_notify(GtkWidget *widget,
				   GdkEventMotion *event,
				   GtkWidget *view)
{
  on_icon = TRUE;
  gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(view),
			     GTK_TEXT_WINDOW_TEXT), hand_cursor);
  return FALSE;
}

static gboolean view_leave_notify(GtkWidget *widget,
				   GdkEventMotion *event,
				   GtkWidget *view)
{
  on_icon = FALSE;
  gdk_window_set_cursor(gtk_text_view_get_window(GTK_TEXT_VIEW(view),
			     GTK_TEXT_WINDOW_TEXT), text_cursor);
  return FALSE;
}

static GtkWidget *weather_channel_evt = NULL;
static void view_scrolled_cb (GtkAdjustment *adj, GtkWidget *view)
{
  if (weather_channel_evt) {
    gint x, y, x1, y1;
    x1 = view->allocation.width - 73 - 5;
    y1 = view->requisition.height - 55 - 5;
    gtk_text_view_buffer_to_window_coords(
			GTK_TEXT_VIEW(view),
			GTK_TEXT_WINDOW_TEXT, x1, y1, &x, &y);
    gtk_text_view_move_child(GTK_TEXT_VIEW(view),
			weather_channel_evt, x, y);
  }
}

static void view_size_allocate_cb	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 gpointer	 data)
{
  view_scrolled_cb(NULL, GTK_WIDGET(data));
}

static gchar *get_logo_path (void)
{
	gchar *dir = g_strconcat(g_get_user_cache_dir(), G_DIR_SEPARATOR_S,
				"xfce4", G_DIR_SEPARATOR_S, "weather-plugin", NULL);

	g_mkdir_with_parents(dir, 0755);
	g_free(dir);

	return g_strconcat(g_get_user_cache_dir(), G_DIR_SEPARATOR_S,
				"xfce4", G_DIR_SEPARATOR_S, "weather-plugin",
				G_DIR_SEPARATOR_S, "weather_logo.jpg", NULL);
}

static void
logo_fetched (gboolean  succeed,
           gchar     *result,
	   size_t    len,
           gpointer  user_data)
{
	if (succeed && result) {
		gchar *path = get_logo_path();
		GError *error = NULL;
		GdkPixbuf *pixbuf = NULL;
		if (!g_file_set_contents(path, result, len, &error)) {
			printf("err %s\n", error?error->message:"?");
			g_error_free(error);
			g_free(result);
			g_free(path);
			return;
		}
		g_free(result);
		pixbuf = gdk_pixbuf_new_from_file(path, NULL);
		g_free(path);
		if (pixbuf) {
			gtk_image_set_from_pixbuf(GTK_IMAGE(user_data), pixbuf);
			g_object_unref(pixbuf);
		}
	}
}

static GtkWidget *weather_summary_get_logo(xfceweather_data *data)
{
	GtkWidget *image = gtk_image_new();
	GdkPixbuf *pixbuf = NULL;
	gchar *path = get_logo_path();

	pixbuf = gdk_pixbuf_new_from_file(path, NULL);
	g_free(path);
	if (pixbuf == NULL) {
		weather_http_receive_data ("xoap.weather.com", "/web/common/twc/logos/web_73x55.jpg",
			data->proxy_host, data->proxy_port, logo_fetched, image);
	} else {
		gtk_image_set_from_pixbuf(GTK_IMAGE(image), pixbuf);
		g_object_unref(pixbuf);
	}

	return image;
}

static GtkWidget *
create_summary_tab (xfceweather_data *data)
{
  GtkTextBuffer *buffer;
  GtkTextIter    iter;
  GtkTextTag    *btag, *ltag1;
  gchar         *value, *wind, *sun_val, *vis;
  GtkWidget     *view, *frame, *scrolled;
  GdkColor       lnk_color;
  GtkAdjustment *adj;
  GtkWidget     *weather_channel_icon;
  gchar         *start;
  xml_time      *timeslice;

  view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  frame = gtk_frame_new (NULL);
  scrolled = gtk_scrolled_window_new (NULL, NULL);

  gtk_container_add (GTK_CONTAINER (scrolled), view);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  gtk_container_set_border_width (GTK_CONTAINER (frame), BORDER);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (frame), scrolled);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
  gtk_text_buffer_get_iter_at_offset (GTK_TEXT_BUFFER (buffer), &iter, 0);
  btag =
    gtk_text_buffer_create_tag (buffer, NULL, "weight", PANGO_WEIGHT_BOLD,
                                NULL);

  gdk_color_parse("#0000ff", &lnk_color);
  ltag1 = gtk_text_buffer_create_tag(buffer, "lnk1", "foreground-gdk", &lnk_color, NULL);

  /* head */
  value = g_strdup_printf (_("Weather report for: %s.\n\n"), data->location_name);
  APPEND_BTEXT (value);
  g_free (value);

  timeslice = get_current_timeslice(data->weatherdata, FALSE);
  value = g_strdup_printf (_("Coordinates: lat %s lon %s\nData applies to: %s.\n"),
                           data->lat, data->lon, ctime(&timeslice->start));
  APPEND_TEXT_ITEM_REAL (value);

  /* Temperature */
  APPEND_BTEXT (_("\nTemperature\n"));
  APPEND_TEXT_ITEM (_("Temperature"), TEMPERATURE);

  /* Wind */
  APPEND_BTEXT (_("\nWind\n"));
  wind = translate_wind_speed (data->weatherdata, get_data (data->weatherdata, WIND_SPEED), data->unit);
  value = g_strdup_printf ("\t%s: %s (%s on the Beaufort scale)\n", _("Speed"), wind,
		get_data (data->weatherdata, WIND_BEAUFORT));
  g_free (wind);
  APPEND_TEXT_ITEM_REAL (value);

  wind = translate_wind_direction (get_data (data->weatherdata, WIND_DIRECTION));
  value = g_strdup_printf ("\t%s: %s (%s%s)\n", _("Direction"),
                           wind ? wind : get_data (data->weatherdata, WIND_DIRECTION),
			   get_data (data->weatherdata, WIND_DIRECTION_DEG),
			   get_unit (data->weatherdata, data->unit, WIND_DIRECTION_DEG));
  g_free (wind);
  APPEND_TEXT_ITEM_REAL (value);

  /* Atmospheric pressure */
  APPEND_BTEXT (_("\nAtmospheric pressure\n"));
  APPEND_TEXT_ITEM (_("Pressure"), PRESSURE);

  /* Other */
  APPEND_BTEXT (_("\nOther\n"));
  APPEND_TEXT_ITEM (_("Humidity"), HUMIDITY);
  APPEND_TEXT_ITEM (_("Low clouds"), CLOUDINESS_LOW);
  APPEND_TEXT_ITEM (_("Medium clouds"), CLOUDINESS_MED);
  APPEND_TEXT_ITEM (_("High clouds"), CLOUDINESS_HIGH);
  APPEND_TEXT_ITEM (_("Fog"), FOG);

  APPEND_BTEXT (_("\nData from The Norwegian Meteorological Institute\n"));
  APPEND_LINK_ITEM ("\t", "Thanks to met.no", "http://met.no/", ltag1);

  g_signal_connect(G_OBJECT(view), "motion-notify-event",
		   G_CALLBACK(view_motion_notify), view);
  g_signal_connect(G_OBJECT(view), "leave-notify-event",
		   G_CALLBACK(view_leave_notify), view);

  if (hand_cursor == NULL)
    hand_cursor = gdk_cursor_new(GDK_HAND2);
  if (text_cursor == NULL)
    text_cursor = gdk_cursor_new(GDK_XTERM);


  return frame;
}

static GtkWidget *
make_forecast (xml_weather *weatherdata,
               units     unit)
{
  GtkWidget *item_vbox, *temp_hbox, *icon_hbox,
            *label, *icon_d, *icon_n, *box_d, *box_n;
  GdkPixbuf *icon;
  gchar     *str, *day, *wind;

  item_vbox = gtk_vbox_new (FALSE, 0);
#if 0
  DBG ("this day %s", weatherdata->day);

  gtk_container_set_border_width (GTK_CONTAINER (item_vbox), 6);


  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);

  day = translate_day (get_data_f (weatherdata, WDAY));
  str = g_strdup_printf ("<b>%s</b>", day ? day : get_data_f (weatherdata, WDAY));
  g_free (day);

  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);

  gtk_box_pack_start (GTK_BOX (item_vbox), label, FALSE, FALSE, 0);

  icon_hbox = gtk_hbox_new (FALSE, 0);

  icon = get_icon (get_data_f (weatherdata, ICON_D), 48);
  icon_d = gtk_image_new_from_pixbuf (icon);
  box_d = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (box_d), FALSE);
  gtk_container_add (GTK_CONTAINER (box_d), icon_d);

  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));

  icon = get_icon (get_data_f (weatherdata, ICON_N), 48);
  icon_n = gtk_image_new_from_pixbuf (icon);
  box_n = gtk_event_box_new ();
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (box_n), FALSE);
  gtk_container_add (GTK_CONTAINER (box_n), icon_n);

  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));

  if (G_UNLIKELY (!tooltips))
    tooltips = gtk_tooltips_new ();

  str = g_strdup_printf (_("Day: %s"),
                         translate_desc (get_data_f (weatherdata, TRANS_D)));
  gtk_tooltips_set_tip (tooltips, box_d, str, NULL);
  g_free (str);

  str = g_strdup_printf (_("Night: %s"),
                         translate_desc (get_data_f (weatherdata, TRANS_N)));
  gtk_tooltips_set_tip (tooltips, box_n, str, NULL);
  g_free (str);

  gtk_box_pack_start (GTK_BOX (icon_hbox), box_d, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (icon_hbox), box_n, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (item_vbox), icon_hbox, FALSE, FALSE, 0);

  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  gtk_label_set_markup (GTK_LABEL (label), _("<b>Precipitation</b>"));
  gtk_box_pack_start (GTK_BOX (item_vbox), label, FALSE, FALSE, 0);

  temp_hbox = gtk_hbox_new (FALSE, 0);
  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  str = g_strdup_printf ("%s %%", get_data_f (weatherdata, PPCP_D));
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
  gtk_box_pack_start (GTK_BOX (temp_hbox), label, TRUE, TRUE, 0);

  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  str = g_strdup_printf ("%s %%", get_data_f (weatherdata, PPCP_N));
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);

  gtk_box_pack_start (GTK_BOX (temp_hbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (item_vbox), temp_hbox, FALSE, FALSE, 0);

  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  gtk_label_set_markup (GTK_LABEL (label), _("<b>Temperature</b>"));
  gtk_box_pack_start (GTK_BOX (item_vbox), label, FALSE, FALSE, 0);

  temp_hbox = gtk_hbox_new (FALSE, 0);
  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  str = g_strdup_printf ("<span foreground=\"red\">%s</span> %s",
                         get_data_f (weatherdata, TEMP_MAX), get_unit (unit,
                                                                       TEMP));
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
  gtk_box_pack_start (GTK_BOX (temp_hbox), label, TRUE, TRUE, 0);
  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  str = g_strdup_printf ("<span foreground=\"blue\">%s</span> %s",
                         get_data_f (weatherdata, TEMP_MIN), get_unit (unit,
                                                                       TEMP));
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
  gtk_box_pack_start (GTK_BOX (temp_hbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (item_vbox), temp_hbox, FALSE, FALSE, 0);

  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  gtk_label_set_markup (GTK_LABEL (label), _("<b>Wind</b>"));
  gtk_box_pack_start (GTK_BOX (item_vbox), label, FALSE, FALSE, 0);

  temp_hbox = gtk_hbox_new (FALSE, 0);
  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);

  wind = translate_wind_direction (get_data_f (weatherdata, W_DIRECTION_D));
  gtk_label_set_markup (GTK_LABEL (label),
                        wind ? wind : get_data_f (weatherdata,
                                                  W_DIRECTION_D));
  g_free (wind);

  gtk_box_pack_start (GTK_BOX (temp_hbox), label, TRUE, TRUE, 0);

  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);

  wind = translate_wind_direction (get_data_f (weatherdata, W_DIRECTION_N));
  gtk_label_set_markup (GTK_LABEL (label),
                        wind ? wind : get_data_f (weatherdata,
                                                  W_DIRECTION_N));
  g_free (wind);

  gtk_box_pack_start (GTK_BOX (temp_hbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (item_vbox), temp_hbox, FALSE, FALSE, 0);

  /* speed */
  temp_hbox = gtk_hbox_new (FALSE, 2);
  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  str = g_strdup_printf ("%s %s", get_data_f (weatherdata, W_SPEED_D),
                         get_unit (unit, WIND_SPEED));
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
  gtk_box_pack_start (GTK_BOX (temp_hbox), label, TRUE, TRUE, 0);

  label = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (label), 0.5, 0.5);
  str = g_strdup_printf ("%s %s", get_data_f (weatherdata, W_SPEED_N),
                         get_unit (unit, WIND_SPEED));
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
  gtk_box_pack_start (GTK_BOX (temp_hbox), label, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (item_vbox), temp_hbox, FALSE, FALSE, 0);

  DBG ("Done");
#endif
  return item_vbox;
}



static GtkWidget *
create_forecast_tab (xfceweather_data *data)
{
  GtkWidget *widg = gtk_hbox_new (FALSE, 0);
  guint      i;

  gtk_container_set_border_width (GTK_CONTAINER (widg), 6);
#if 0
  if (data->weatherdata && data->weatherdata->dayf)
    {
      for (i = 0; i < XML_WEATHER_DAYF_N - 1; i++)
        {
          if (!data->weatherdata->dayf[i])
            break;

          DBG ("%s", data->weatherdata->dayf[i]->day);

          gtk_box_pack_start (GTK_BOX (widg),
                              make_forecast (data->weatherdata->dayf[i], data->unit), FALSE,
                              FALSE, 0);
          gtk_box_pack_start (GTK_BOX (widg), gtk_vseparator_new (), TRUE,
                              TRUE, 0);
        }

      if (data->weatherdata->dayf[i])
        gtk_box_pack_start (GTK_BOX (widg),
                            make_forecast (data->weatherdata->dayf[i], data->unit), FALSE, FALSE,
                            0);
    }
#endif
  return widg;
}

static void
summary_dialog_response (GtkWidget          *dlg,
                         gint                response,
                         GtkWidget          *window)
{
	if (response == GTK_RESPONSE_ACCEPT)
		gtk_widget_destroy(window);
	else if (response == GTK_RESPONSE_HELP)
		g_spawn_command_line_async ("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);
}

GtkWidget *
create_summary_window (xfceweather_data *data)
{
  GtkWidget *window, *notebook, *vbox;
  gchar     *title;
  GdkPixbuf *icon;

  window = xfce_titled_dialog_new_with_buttons (_("Weather Update"),
                                                NULL,
                                                GTK_DIALOG_NO_SEPARATOR,
                                                GTK_STOCK_ABOUT,
						GTK_RESPONSE_HELP,
                                                GTK_STOCK_CLOSE,
                                                GTK_RESPONSE_ACCEPT, NULL);

  title = g_strdup_printf (_("Weather report for: %s"), data->location_name);

  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (window), title);
  g_free (title);

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, TRUE, TRUE,
                      0);

  icon = get_icon (get_data (data->weatherdata, SYMBOL), 48);

  if (!icon)
    icon = get_icon ("99", 48);

  gtk_window_set_icon (GTK_WINDOW (window), icon);

  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));

  notebook = gtk_notebook_new ();
  gtk_container_set_border_width (GTK_CONTAINER (notebook), BORDER);
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            create_forecast_tab (data),
                            gtk_label_new (_("Forecast")));
  gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                            create_summary_tab (data),
                            gtk_label_new (_("Details")));

  gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);

  g_signal_connect (G_OBJECT (window), "response",
                    G_CALLBACK (summary_dialog_response), window);

  gtk_window_set_default_size (GTK_WINDOW (window), 500, 400);

  return window;
}
