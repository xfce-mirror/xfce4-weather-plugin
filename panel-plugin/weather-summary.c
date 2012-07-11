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
#define APPEND_TEXT_ITEM(text, item)     rawvalue = get_data(conditions, data->unit_system, item); \
                                         unit = get_unit(conditions, data->unit_system, item); \
                                         value = g_strdup_printf("\t%s%s%s%s%s\n", \
                                                                 text, text ? ": " : "", \
                                                                 rawvalue, \
                                                                 strcmp(unit, "°") ? " " : "", \
                                                                 unit); \
                                         g_free (rawvalue);\
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
  gchar         *value, *wind, *sun_val, *vis, *rawvalue;
  const gchar   *unit;
  GtkWidget     *view, *frame, *scrolled;
  GdkColor       lnk_color;
  GtkAdjustment *adj;
  GtkWidget     *weather_channel_icon;
  xml_time      *conditions;
  struct tm     *start, *end, *point_tm;
  char           interval_start[80], interval_end[80], point[80];

  view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (view), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (view), BORDER);
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

  conditions = get_current_conditions(data->weatherdata);
  APPEND_BTEXT(_("Coordinates\n"));
  APPEND_TEXT_ITEM (_("Altitude"), ALTITUDE);
  APPEND_TEXT_ITEM (_("Latitude"), LATITUDE);
  APPEND_TEXT_ITEM (_("Longitude"), LONGITUDE);

  APPEND_BTEXT(_("\nTime\n"));
  point_tm = localtime(&conditions->point);
  strftime (point, 80, "%c", point_tm);
  value = g_strdup_printf (_("\tPoint data applies to:\n\t%s\n"), point);
  APPEND_TEXT_ITEM_REAL (value);

  start = localtime(&conditions->start);
  strftime (interval_start, 80, "%c", start);
  end = localtime(&conditions->end);
  strftime (interval_end, 80, "%c", end);
  value = g_strdup_printf (_("\n\tInterval data applies to:\n\t%s\n\t%s\n"),
                           interval_start,
                           interval_end);
  APPEND_TEXT_ITEM_REAL (value);

  value = g_strdup_printf (_("\n\tInterval data provides icon, description and precipitation,"
                             "\n\twhile point data supplies the rest.\n"), point);
  APPEND_TEXT_ITEM_REAL (value);


  /* Temperature */
  APPEND_BTEXT (_("\nTemperature\n"));
  APPEND_TEXT_ITEM (_("Temperature"), TEMPERATURE);

  /* Wind */
  APPEND_BTEXT (_("\nWind\n"));
  rawvalue = get_data (conditions, data->unit_system, WIND_SPEED);
  wind = translate_wind_speed (conditions, rawvalue, data->unit_system);
  g_free (rawvalue);
  rawvalue = get_data (conditions, data->unit_system, WIND_BEAUFORT);
  value = g_strdup_printf (_("\t%s: %s (%s on the Beaufort scale)\n"), _("Speed"), wind, rawvalue);
  g_free (rawvalue);
  g_free (wind);
  APPEND_TEXT_ITEM_REAL (value);

  rawvalue = get_data (conditions, data->unit_system, WIND_DIRECTION);
  wind = translate_wind_direction (rawvalue);
  g_free (rawvalue);
  rawvalue = get_data (conditions, data->unit_system, WIND_DIRECTION_DEG);
  value = g_strdup_printf ("\t%s: %s (%s%s)\n", _("Direction"),
                           wind, rawvalue,
                           get_unit (conditions, data->unit_system, WIND_DIRECTION_DEG));
  g_free (rawvalue);
  g_free (wind);
  APPEND_TEXT_ITEM_REAL (value);

  /* Precipitation */
  APPEND_BTEXT (_("\nPrecipitations\n"));
  APPEND_TEXT_ITEM(_("Precipitations amount"), PRECIPITATIONS);

  /* Atmosphere */
  APPEND_BTEXT (_("\nAtmosphere\n"));
  APPEND_TEXT_ITEM (_("Pressure"), PRESSURE);
  APPEND_TEXT_ITEM (_("Humidity"), HUMIDITY);

  /* Clouds */
  APPEND_BTEXT (_("\nClouds\n"));
  APPEND_TEXT_ITEM (_("Fog"), FOG);
  APPEND_TEXT_ITEM (_("Low clouds"), CLOUDINESS_LOW);
  APPEND_TEXT_ITEM (_("Medium clouds"), CLOUDINESS_MED);
  APPEND_TEXT_ITEM (_("High clouds"), CLOUDINESS_HIGH);
  APPEND_TEXT_ITEM (_("Cloudiness"), CLOUDINESS_OVERALL);

  APPEND_BTEXT (_("\nData from The Norwegian Meteorological Institute\n"));
  APPEND_LINK_ITEM ("\t", _("Thanks to met.no"), "http://met.no/", ltag1);

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

GtkWidget *
add_forecast_cell(GtkWidget *widget, GdkColor *color) {
    GtkWidget *ebox;
    ebox = gtk_event_box_new();
    if (color == NULL)
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), FALSE);
    else {
        gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), TRUE);
        gtk_widget_modify_bg(GTK_WIDGET(ebox), GTK_STATE_NORMAL, color);
    }
    gtk_widget_show(GTK_WIDGET(ebox));
    gtk_container_add(GTK_CONTAINER(ebox), GTK_WIDGET(widget));
    return ebox;
}

GtkWidget *
add_forecast_header(gchar *text, gdouble angle, GdkColor *color)
{
    GtkWidget *label, *align;
    gchar *str;

	if (angle)
		align = gtk_alignment_new(1, 1, 0, 1);
	else
		align = gtk_alignment_new(1, 1, 1, 0);
	gtk_container_set_border_width(GTK_CONTAINER(align), 4);
    gtk_widget_show(GTK_WIDGET(align));

    label = gtk_label_new(NULL);
    gtk_label_set_angle(GTK_LABEL(label), angle);
    gtk_widget_show(GTK_WIDGET(label));
    str = g_strdup_printf("<span foreground=\"white\"><b>%s</b></span>", text ? text : "");
    gtk_label_set_markup(GTK_LABEL(label), str);
    g_free(str);
    gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(label));

    return add_forecast_cell(align, color);
}

static GtkWidget *
make_forecast (xfceweather_data *data)
{
    GtkWidget *table, *ebox, *box, *align;
    GtkWidget *forecast_box, *label, *image;
    GdkPixbuf *icon;
    GdkColor lightbg = {0, 0xeaea, 0xeaea, 0xeaea};
    GdkColor darkbg = {0, 0x6666, 0x6666, 0x6666};
    gint i, weekday, daytime;
    gchar *dayname, *wind_speed, *value, *rawvalue;
    xml_time *fcdata;
    time_t now_t = time(NULL), fcday_t;
    struct tm tm_fcday;

    table = gtk_table_new(data->forecast_days + 1, 5, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 0);
    gtk_table_set_col_spacings(GTK_TABLE(table), 0);
    gtk_widget_show(GTK_WIDGET(table));

    /* empty upper left corner */
    box = gtk_vbox_new(FALSE, 0);
    gtk_widget_show(GTK_WIDGET(box));
    gtk_table_attach_defaults(GTK_TABLE(table),
                              add_forecast_cell(box, &darkbg),
                              0, 1, 0, 1);

    /* daytime headers */
    gtk_table_attach_defaults(GTK_TABLE(table),
                              add_forecast_header(_("Morning"), 0.0, &darkbg),
                              1, 2, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              add_forecast_header(_("Afternoon"), 0.0, &darkbg),
                              2, 3, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              add_forecast_header(_("Evening"), 0.0, &darkbg),
                              3, 4, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table),
                              add_forecast_header(_("Night"), 0.0, &darkbg),
                              4, 5, 0, 1);

    for (i = 0; i < data->forecast_days; i++) {
        /* Forecast day headers */
        tm_fcday = *localtime(&now_t);
        fcday_t = time_calc_day(tm_fcday, i);
        weekday = localtime(&fcday_t)->tm_wday;
        if (i == 0)
            dayname = _("Today");
        else if (i == 1)
            dayname = _("Tomorrow");
        else
            dayname = translate_day(weekday);

        ebox = add_forecast_header(dayname, 90.0, &darkbg);

        gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(ebox),
                                  0, 1, i+1, i+2);

        /* Get forecast data for each daytime */
        for (daytime = MORNING; daytime <= NIGHT; daytime++) {
            forecast_box = gtk_vbox_new(FALSE, 0);
            align = gtk_alignment_new(0.5, 0.5, 1, 1);
            gtk_container_set_border_width(GTK_CONTAINER(align), 4);
            gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(forecast_box));
            if (i % 2)
                ebox = add_forecast_cell(align, NULL);
            else
                ebox = add_forecast_cell(align, &lightbg);

            fcdata = make_forecast_data(data->weatherdata, i, daytime);
            if (fcdata != NULL) {
                if (fcdata->location != NULL) {
                    rawvalue = get_data(fcdata, data->unit_system, SYMBOL);
                    icon = get_icon(rawvalue, 48, (daytime == NIGHT));
                    g_free(rawvalue);
                    image = gtk_image_new_from_pixbuf(icon);
                    gtk_box_pack_start(GTK_BOX(forecast_box), GTK_WIDGET(image),
                                        TRUE, TRUE, 0);
                    if (G_LIKELY (icon))
                        g_object_unref (G_OBJECT (icon));

                    rawvalue = get_data(fcdata, data->unit_system, SYMBOL);
                    value = g_strdup_printf("%s",
                                            translate_desc(rawvalue,
                                                           (daytime == NIGHT)));
                    g_free(rawvalue);
                    label = gtk_label_new(NULL);
                    gtk_label_set_markup(GTK_LABEL(label), value);
                    gtk_widget_show(GTK_WIDGET(label));
                    gtk_box_pack_start(GTK_BOX(forecast_box), GTK_WIDGET(label),
                                        TRUE, TRUE, 0);
                    g_free(value);

                    rawvalue = get_data(fcdata, data->unit_system, TEMPERATURE);
                    value = g_strdup_printf("%s %s",
                                            rawvalue,
                                            get_unit(fcdata, data->unit_system, TEMPERATURE));
                    g_free(rawvalue);
                    label = gtk_label_new(value);
                    gtk_widget_show(GTK_WIDGET(label));
                    gtk_box_pack_start(GTK_BOX(forecast_box), GTK_WIDGET(label),
                                        TRUE, TRUE, 0);
                    g_free(value);

                    rawvalue = get_data(fcdata, data->unit_system, WIND_DIRECTION);
                    wind_speed = get_data(fcdata, data->unit_system, WIND_SPEED);
                    value = g_strdup_printf("%s %s %s",
                                            translate_wind_direction(rawvalue),
                                            wind_speed,
                                            get_unit(fcdata, data->unit_system, WIND_SPEED));
                    g_free(wind_speed);
                    g_free(rawvalue);
                    label = gtk_label_new(value);
                    gtk_widget_show(GTK_WIDGET(label));
                    gtk_box_pack_start(GTK_BOX(forecast_box), label, TRUE, TRUE, 0);
                    g_free(value);
                }
                xml_time_free(fcdata);
            }
            gtk_table_attach_defaults(GTK_TABLE(table),
                                      GTK_WIDGET(ebox),
                                      1+daytime, 2+daytime, i+1, i+2);
        }
    }
    return table;
}



static GtkWidget *
create_forecast_tab (xfceweather_data *data, GtkWidget *window)
{
    GtkWidget   *align, *hbox, *scrolled, *table;
    GdkWindow   *win;
    GdkScreen   *screen;
    GdkRectangle rect;
    gint         monitor_num, height_needed, height_max;

    /* calculate maximum height we may use, subtracting some sane value for safety */
    screen = gtk_window_get_screen(GTK_WINDOW(window));
    win = GTK_WIDGET(window)->window;
    monitor_num = gdk_screen_get_monitor_at_window(GDK_SCREEN(screen), GDK_WINDOW(win));
    gdk_screen_get_monitor_geometry(GDK_SCREEN(screen), monitor_num, &rect);
    height_max = rect.height * 0.85 - 200;

    /* calculate height needed using a good arbitrary value */
    height_needed = data->forecast_days * 110;

    /* generate the forecast table */
    table = GTK_WIDGET(make_forecast(data));

    align = gtk_alignment_new(0.5, 0, 0.5, 0);
    if (height_needed < height_max) {
        gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(table));
        gtk_container_set_border_width(GTK_CONTAINER(align), BORDER);
        return align;
    } else {
        hbox = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), table, TRUE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(hbox));

        scrolled = gtk_scrolled_window_new (NULL, NULL);
        gtk_container_set_border_width(GTK_CONTAINER(scrolled), BORDER);

        gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled), align);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_widget_set_size_request(GTK_WIDGET(scrolled), 550, height_max);
        return scrolled;
    }
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
  GtkWidget *window, *notebook, *vbox, *hbox, *label;
  gchar     *title, *rawvalue;
  GdkPixbuf *icon;
  xml_time  *conditions;

  window = xfce_titled_dialog_new_with_buttons (_("Weather Update"),
                                                NULL,
                                                GTK_DIALOG_NO_SEPARATOR,
                                                GTK_STOCK_ABOUT,
						GTK_RESPONSE_HELP,
                                                GTK_STOCK_CLOSE,
                                                GTK_RESPONSE_ACCEPT, NULL);
  if (data->location_name_short != NULL) {
    title = g_strdup_printf (_("Weather report for: %s"), data->location_name_short);
    xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (window), title);
    g_free (title);
  }

  vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (window)->vbox), vbox, TRUE, TRUE,
                      0);

  conditions = get_current_conditions(data->weatherdata);

  rawvalue = get_data (conditions, data->unit_system, SYMBOL);
  icon = get_icon (rawvalue, 48, is_night_time());
  g_free (rawvalue);

  gtk_window_set_icon (GTK_WINDOW (window), icon);

  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));

  if (data->location_name == NULL || data->weatherdata == NULL) {
    hbox = gtk_hbox_new (FALSE, 0);
    gtk_widget_show(hbox);
    if (data->location_name == NULL)
      label = gtk_label_new(_("Please set a location in the plugin settings."));
    else
      label = gtk_label_new(_("Currently no data available."));
    gtk_widget_show(label);
    gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET(label),
                        TRUE, TRUE, 0);

    gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET(hbox),
                        TRUE, TRUE, 0);
    gtk_window_set_default_size (GTK_WINDOW (window), 500, 400);
  } else {
    notebook = gtk_notebook_new ();
    gtk_container_set_border_width (GTK_CONTAINER (notebook), BORDER);
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                              create_forecast_tab (data, window),
                              gtk_label_new (_("Forecast")));
    gtk_notebook_append_page (GTK_NOTEBOOK (notebook),
                              create_summary_tab (data),
                              gtk_label_new (_("Details")));

    gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
  }

  g_signal_connect (G_OBJECT (window), "response",
                    G_CALLBACK (summary_dialog_response), window);

  return window;
}
