/*   Copyright (c) 2003-2007 Xfce Development Team
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

#include <string.h>
#include <sys/stat.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#include "weather-translate.h"
#include "weather-http.h"
#include "weather-summary.h"
#include "weather-config.h"
#include "weather-icon.h"
#include "weather-scrollbox.h"

#define XFCEWEATHER_ROOT "weather"
#define UPDATE_TIME      1600
#define BORDER           8

#if !GLIB_CHECK_VERSION(2,14,0)
#define g_timeout_add_seconds(t,c,d) g_timeout_add((t)*1000,(c),(d))
#endif

gboolean
check_envproxy (gchar **proxy_host,
                gint   *proxy_port)
{

  gchar *env_proxy = NULL, *tmp, **split;

  env_proxy = getenv ("HTTP_PROXY");
  if (!env_proxy)
    env_proxy = getenv ("http_proxy");

  if (!env_proxy)
    return FALSE;

  tmp = strstr (env_proxy, "://");

  if (!tmp)
    tmp = env_proxy;
  else if (strlen (tmp) >= 3)
    env_proxy = tmp + 3;
  else
    return FALSE;

  /* we don't support username:password so return */
  tmp = strchr (env_proxy, '@');
  if (tmp)
    return FALSE;

  split = g_strsplit (env_proxy, ":", 2);

  if (!split[0])
    return FALSE;
  else if (!split[1])
    {
      g_strfreev (split);
      return FALSE;
    }

  *proxy_host = g_strdup (split[0]);
  *proxy_port = (int) strtol (split[1], NULL, 0);

  g_strfreev (split);

  return TRUE;
}

static gchar *
get_label_size (xfceweather_data *data)
{
#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
  /* use small label with low number of columns in deskbar mode */
  if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_DESKBAR)
    if (data->panel_size > 99)
      return "medium";
    else if (data->panel_size > 79)
      return "small";
    else
      return "x-small";
  else
#endif
  if (data->panel_size > 25)
    return "medium";
  else if (data->panel_size > 23)
    return "small";
  else
    return "x-small";
}

static gchar *
make_label (xfceweather_data *data,
            datas             opt)
{
  gchar       *str, *value, *rawvalue;
  xml_time    *conditions;
  const gchar *lbl, *txtsize;

  switch (opt)
    {
    case TEMPERATURE:
      lbl = _("T");
      break;
    case PRESSURE:
      lbl = _("P");
      break;
    case WIND_SPEED:
      lbl = _("WS");
      break;
    case WIND_DIRECTION:
      lbl = _("WD");
      break;
    case HUMIDITY:
      lbl = _("H");
      break;
    case CLOUDINESS_LOW:
      lbl = _("CL");
      break;
    case CLOUDINESS_MED:
      lbl = _("CM");
      break;
    case CLOUDINESS_HIGH:
      lbl = _("CH");
      break;
    case CLOUDINESS_OVERALL:
      lbl = _("CO");
      break;
    case FOG:
      lbl = _("F");
      break;
    case PRECIPITATIONS:
      lbl = _("R");
      break;
    default:
      lbl = "?";
      break;
    }

  txtsize = get_label_size(data);

  /* get current weather conditions */
  conditions = get_current_conditions(data->weatherdata);
  rawvalue = get_data(conditions, data->unit, opt);

  switch (opt)
    {
    case WIND_DIRECTION:
      value = translate_wind_direction (rawvalue);
      break;
    case WIND_SPEED:
      value = translate_wind_speed (conditions, rawvalue, data->unit);
      break;
    default:
      value = NULL;
      break;
    }

  if (data->labels->len > 1) {
    if (value != NULL)
      {
	str = g_strdup_printf ("<span size=\"%s\">%s: %s</span>",
                               txtsize, lbl, value);
	g_free (value);
      }
    else
      {
	str = g_strdup_printf ("<span size=\"%s\">%s: %s %s</span>",
                               txtsize, lbl, rawvalue, get_unit (conditions, data->unit, opt));
      }
  } else {
    if (value != NULL)
      {
	str = g_strdup_printf ("<span size=\"%s\">%s</span>",
                               txtsize, value);
	g_free (value);
      }
    else
      {
	str = g_strdup_printf ("<span size=\"%s\">%s %s</span>",
                               txtsize, rawvalue, get_unit (conditions, data->unit, opt));
      }
  }
  g_free (rawvalue);
  return str;
}



static void
set_icon_error (xfceweather_data *data)
{
  GdkPixbuf   *icon;
  gint         height;
  gint         size;
  gchar       *str;
  const gchar *txtsize;

  size = data->panel_size;

  if (data->weatherdata)
    {
      xml_weather_free (data->weatherdata);
      data->weatherdata = NULL;
    }

  txtsize = get_label_size(data);

  str = g_strdup_printf ("<span size=\"%s\">No Data</span>", txtsize);
  gtk_scrollbox_set_label (GTK_SCROLLBOX (data->scrollbox), -1, str);
  g_free (str);

  gtk_widget_get_size_request (data->scrollbox, NULL, &height);

#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
  /* make icon double-size in deskbar mode */
  if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_DESKBAR
      && data->size != data->panel_size)
    icon = get_icon ("99", data->size * 2, FALSE);
  else
    icon = get_icon ("99", data->size, FALSE);
#else
  if (data->panel_orientation == GTK_ORIENTATION_VERTICAL)
    icon = get_icon ("99", data->size - height - 2, FALSE);
  else
    icon = get_icon ("99", data->size, FALSE);
#endif

  gtk_image_set_from_pixbuf (GTK_IMAGE (data->iconimage), icon);

  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));

#if !GTK_CHECK_VERSION(2,12,0)
  gtk_tooltips_set_tip (data->tooltips, data->tooltipbox,
                        _("Cannot update weather data"), NULL);
#endif
}



static void
set_icon_current (xfceweather_data *data)
{
  xml_time       *conditions;
  guint           i;
  GdkPixbuf      *icon = NULL;
  datas           opt;
  gchar          *str;
  gint            size, height;
  gboolean        nighttime;

  for (i = 0; i < data->labels->len; i++)
    {
      opt = g_array_index (data->labels, datas, i);

      str = make_label (data, opt);

      gtk_scrollbox_set_label (GTK_SCROLLBOX (data->scrollbox), -1, str);

      g_free (str);
    }

  if (i == 0)
    {
      size = data->size;
    }
  else
    {
      gtk_widget_get_size_request (data->scrollbox, NULL, &height);
#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
      /* make icon double-size in deskbar mode */
      if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_DESKBAR
          && data->size != data->panel_size)
        size = data->size * 2;
      else
        size = data->size;
#else
      if (data->panel_orientation == GTK_ORIENTATION_VERTICAL)
        size = data->size - height - 2;
      else
        size = data->size;
#endif
    }
 
  /* get current weather conditions */
  conditions = get_current_conditions(data->weatherdata);
  nighttime = is_night_time();

  str = get_data (conditions, data->unit, SYMBOL);
  icon = get_icon (str, size, nighttime);
  g_free (str);
 
  gtk_image_set_from_pixbuf (GTK_IMAGE (data->iconimage), icon);

  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));

#if !GTK_CHECK_VERSION(2,12,0)
  str = get_data (conditions, data->unit, SYMBOL);
  gtk_tooltips_set_tip (data->tooltips, data->tooltipbox,
                        translate_desc (str, nighttime),
                        NULL);
  g_free (str);
#endif
}



static void
cb_update (gboolean  succeed,
           gchar    *result,
	   size_t    len,
           gpointer  user_data)
{
  xfceweather_data *data = user_data;
  xmlDoc           *doc;
  xmlNode          *cur_node;
  xml_weather      *weather = NULL;

  if (G_LIKELY (succeed && result))
    {
      if (g_utf8_validate(result, -1, NULL)) {
	/* force parsing as UTF-8, the XML encoding header may lie */
	doc = xmlReadMemory (result, strlen (result), NULL, "UTF-8", 0);
      } else {
	doc = xmlParseMemory (result, strlen(result));
      }

      g_free (result);

      if (G_LIKELY (doc))
        {
          cur_node = xmlDocGetRootElement (doc);

          if (cur_node)
            weather = parse_weather (cur_node);

          xmlFreeDoc (doc);
        }
    }

  gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));

  if (weather)
    {
      weather->current_conditions = make_current_conditions(weather);
      if (data->weatherdata)
        xml_weather_free (data->weatherdata);

      data->weatherdata = weather;
      set_icon_current (data);
    }
  else
    {
      set_icon_error (data);
    }
}



static gboolean
update_weatherdata (xfceweather_data *data)
{
  gchar *url;

  if (!data->lat || !data->lon)
    {
      gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));
      set_icon_error (data);

      return TRUE;
    }

  /* build url */
  url = g_strdup_printf ("/weatherapi/locationforecastlts/1.1/?lat=%s;lon=%s",
                         data->lat, data->lon);

  /* start receive thread */
  g_warning("getting http://api.yr.no/%s", url);
  weather_http_receive_data ("api.yr.no", url, data->proxy_host,
                             data->proxy_port, cb_update, data);

  /* cleanup */
  g_free (url);

  /* keep timeout running */
  return TRUE;
}



static GArray *
labels_clear (GArray *array)
{
  if (!array || array->len > 0)
    {
      if (array)
        g_array_free (array, TRUE);

      array = g_array_new (FALSE, TRUE, sizeof (datas));
    }

  return array;
}



static void
xfceweather_set_visibility (xfceweather_data *data)
{
  if (data->labels->len > 0)
    gtk_widget_show (data->scrollbox);
  else
    gtk_widget_hide (data->scrollbox);
}



static void
xfceweather_read_config (XfcePanelPlugin  *plugin,
                         xfceweather_data *data)
{
  gchar        label[10];
  guint        i;
  const gchar *value;
  gchar       *file;
  XfceRc      *rc;
  gint         val;

  if (!(file = xfce_panel_plugin_lookup_rc_file (plugin)))
    return;

  rc = xfce_rc_simple_open (file, TRUE);
  g_free (file);

  if (!rc)
    return;

  value = xfce_rc_read_entry (rc, "lat", NULL);

  if (value)
    {
      if (data->lat)
        g_free (data->lat);

      data->lat = g_strdup (value);
    }

  value = xfce_rc_read_entry (rc, "lon", NULL);

  if (value)
    {
      if (data->lon)
        g_free (data->lon);

      data->lon = g_strdup (value);
    }

  value = xfce_rc_read_entry (rc, "loc_name", NULL);

  if (value)
    {
      if (data->location_name)
        g_free (data->location_name);

      data->location_name = g_strdup (value);
    }

  value = xfce_rc_read_entry (rc, "loc_name_short", NULL);

  if (value)
    {
      if (data->location_name_short)
        g_free (data->location_name_short);

      data->location_name_short = g_strdup (value);
    }

  if (xfce_rc_read_bool_entry (rc, "celcius", TRUE))
    data->unit = METRIC;
  else
    data->unit = IMPERIAL;

  if (data->proxy_host)
    {
      g_free (data->proxy_host);
      data->proxy_host = NULL;
    }

  if (data->saved_proxy_host)
    {
      g_free (data->saved_proxy_host);
      data->saved_proxy_host = NULL;
    }

  value = xfce_rc_read_entry (rc, "proxy_host", NULL);

  if (value && *value)
    {
      data->saved_proxy_host = g_strdup (value);
    }

  data->saved_proxy_port = xfce_rc_read_int_entry (rc, "proxy_port", 0);

  data->proxy_fromenv = xfce_rc_read_bool_entry (rc, "proxy_fromenv", FALSE);

  if (data->proxy_fromenv)
    {
      check_envproxy (&data->proxy_host, &data->proxy_port);
    }
  else
    {
      data->proxy_host = g_strdup (data->saved_proxy_host);
      data->proxy_port = data->saved_proxy_port;
    }

  data->animation_transitions = xfce_rc_read_bool_entry (rc,
  		"animation_transitions", TRUE);

  gtk_scrollbox_set_animate(GTK_SCROLLBOX(data->scrollbox), data->animation_transitions);

  data->labels = labels_clear (data->labels);

  for (i = 0; i < 100 /* arbitrary */ ; ++i)
    {
      g_snprintf (label, 10, "label%d", i);

      val = xfce_rc_read_int_entry (rc, label, -1);

      if (val >= 0)
        g_array_append_val (data->labels, val);
      else
        break;
    }

  xfce_rc_close (rc);
}



static void
xfceweather_write_config (XfcePanelPlugin  *plugin,
                          xfceweather_data *data)
{
  gchar   label[10];
  guint   i;
  XfceRc *rc;
  gchar  *file;

  if (!(file = xfce_panel_plugin_save_location (plugin, TRUE)))
    return;

  /* get rid of old values */
  unlink (file);

  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  if (!rc)
    return;

  xfce_rc_write_bool_entry (rc, "celcius", (data->unit == METRIC));

  if (data->lat)
    xfce_rc_write_entry (rc, "lat", data->lat);

  if (data->lon)
    xfce_rc_write_entry (rc, "lon", data->lon);

  if (data->location_name)
    xfce_rc_write_entry (rc, "loc_name", data->location_name);

  if (data->location_name_short)
    xfce_rc_write_entry (rc, "loc_name_short", data->location_name_short);

  xfce_rc_write_bool_entry (rc, "proxy_fromenv", data->proxy_fromenv);

  if (data->proxy_host)
    {
      xfce_rc_write_entry (rc, "proxy_host", data->proxy_host);

      xfce_rc_write_int_entry (rc, "proxy_port", data->proxy_port);
    }

  xfce_rc_write_bool_entry (rc, "animation_transitions", data->animation_transitions);

  for (i = 0; i < data->labels->len; i++)
    {
      g_snprintf (label, 10, "label%d", i);

      xfce_rc_write_int_entry (rc, label,
                               (gint) g_array_index (data->labels, datas, i));
    }

  xfce_rc_close (rc);
}



static void
update_weatherdata_with_reset (xfceweather_data *data)
{
  if (data->updatetimeout)
    g_source_remove (data->updatetimeout);

  update_weatherdata (data);

  data->updatetimeout =
    g_timeout_add_seconds (UPDATE_TIME, (GSourceFunc) update_weatherdata, data);
}


static void
update_config (xfceweather_data * data)
{
  update_weatherdata_with_reset (data);
}



static void
close_summary (GtkWidget *widget,
               gpointer  *user_data)
{
  xfceweather_data *data = (xfceweather_data *) user_data;

  data->summary_window = NULL;
}



static void
forecast_click (GtkWidget *widget,
          gpointer   user_data)
{
  xfceweather_data *data = (xfceweather_data *) user_data;

  if (data->summary_window != NULL)
      gtk_widget_destroy (data->summary_window);
  else
    {
      data->summary_window = create_summary_window (data);
      g_signal_connect (G_OBJECT (data->summary_window), "destroy",
                	G_CALLBACK (close_summary), data);

      gtk_widget_show_all (data->summary_window);
    }
}

static gboolean
cb_click (GtkWidget      *widget,
          GdkEventButton *event,
          gpointer        user_data)
{
  xfceweather_data *data = (xfceweather_data *) user_data;

  if (event->button == 1)
    {
      forecast_click(widget, user_data);
    }
  else if (event->button == 2)
    {
      update_weatherdata_with_reset (data);
    }

  return FALSE;
}

static gboolean
cb_scroll (GtkWidget      *widget,
           GdkEventScroll *event,
           gpointer        user_data)
{
  xfceweather_data *data = (xfceweather_data *) user_data;

  if (event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_DOWN)
    {
      gtk_scrollbox_next_label(GTK_SCROLLBOX(data->scrollbox));
    }

  return FALSE;
}


static void
mi_click (GtkWidget *widget,
          gpointer   user_data)
{
  xfceweather_data *data = (xfceweather_data *) user_data;

  update_weatherdata_with_reset (data);
}



static void
xfceweather_dialog_response (GtkWidget          *dlg,
                             gint                response,
                             xfceweather_dialog *dialog)
{
  xfceweather_data *data = (xfceweather_data *) dialog->wd;
  gboolean          result;

  if (response == GTK_RESPONSE_HELP)
    {
      /* show help */
      result = g_spawn_command_line_async ("exo-open --launch WebBrowser " PLUGIN_WEBSITE, NULL);

      if (G_UNLIKELY (result == FALSE))
        g_warning (_("Unable to open the following url: %s"), PLUGIN_WEBSITE);
    }
  else
    {
      apply_options (dialog);

      gtk_widget_destroy (dlg);
      gtk_list_store_clear (dialog->mdl_xmloption);
      g_slice_free (xfceweather_dialog, dialog);

      xfce_panel_plugin_unblock_menu (data->plugin);
      xfceweather_write_config (data->plugin, data);

      xfceweather_set_visibility (data);
    }
}



static void
xfceweather_create_options (XfcePanelPlugin  *plugin,
                            xfceweather_data *data)
{
  GtkWidget          *dlg, *vbox;
  xfceweather_dialog *dialog;

  xfce_panel_plugin_block_menu (plugin);

  dlg = xfce_titled_dialog_new_with_buttons (_("Weather Update"),
                                             GTK_WINDOW
                                             (gtk_widget_get_toplevel
                                              (GTK_WIDGET (plugin))),
                                             GTK_DIALOG_DESTROY_WITH_PARENT |
                                             GTK_DIALOG_NO_SEPARATOR,
                                             GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                                             GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                             NULL);

  gtk_container_set_border_width (GTK_CONTAINER (dlg), 2);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "xfce4-settings");

  vbox = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER - 2);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox, TRUE, TRUE, 0);

  dialog = create_config_dialog (data, vbox);

  g_signal_connect (G_OBJECT (dlg), "response",
                    G_CALLBACK (xfceweather_dialog_response), dialog);

  set_callback_config_dialog (dialog, update_config);

  gtk_widget_show (dlg);
}

static gboolean weather_get_tooltip_cb (GtkWidget        *widget,
					gint              x,
					gint              y,
					gboolean          keyboard_mode,
					GtkTooltip       *tooltip,
					xfceweather_data *data)
{
  GdkPixbuf *icon;
  gchar *markup_text, *rawvalue;
  xml_time *conditions;
  gboolean nighttime;

  conditions = get_current_conditions(data->weatherdata);
  nighttime = is_night_time();
  if (data->weatherdata == NULL) {
    gtk_tooltip_set_text (tooltip, _("Cannot update weather data"));
  } else {
    rawvalue = get_data (conditions, data->unit, SYMBOL);
    markup_text = g_markup_printf_escaped(
  	  "<b>%s</b>\n"
	  "%s",
	  data->location_name,
	  translate_desc (rawvalue, nighttime)
	  );
    g_free(rawvalue);
    gtk_tooltip_set_markup (tooltip, markup_text);
    g_free(markup_text);
  }

  rawvalue = get_data (conditions, data->unit, SYMBOL);
  icon = get_icon (rawvalue, 32, nighttime);
  g_free (rawvalue);
  gtk_tooltip_set_icon (tooltip, icon);
  g_object_unref (G_OBJECT(icon));

  return TRUE;
}


static xfceweather_data *
xfceweather_create_control (XfcePanelPlugin *plugin)
{
  xfceweather_data *data = g_slice_new0 (xfceweather_data);
  GtkWidget        *refresh, *mi;
  datas             lbl;
  GdkPixbuf        *icon = NULL;

  data->plugin = plugin;

#if !GTK_CHECK_VERSION(2,12,0)
  data->tooltips = gtk_tooltips_new ();
  g_object_ref (data->tooltips);
  gtk_object_sink (GTK_OBJECT (data->tooltips));
#endif
  data->scrollbox = gtk_scrollbox_new ();

  data->size = xfce_panel_plugin_get_size (plugin);
  icon = get_icon ("99", 16, FALSE);
  data->iconimage = gtk_image_new_from_pixbuf (icon);

  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));

  data->labels = g_array_new (FALSE, TRUE, sizeof (datas));

  data->vbox_center_scrollbox = gtk_vbox_new(FALSE, 0);
  data->top_hbox = gtk_hbox_new (FALSE, 0);
  gtk_misc_set_alignment (GTK_MISC (data->iconimage), 1, 0.5);
  gtk_box_pack_start (GTK_BOX (data->top_hbox), data->iconimage, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (data->vbox_center_scrollbox), data->scrollbox, TRUE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (data->top_hbox), data->vbox_center_scrollbox, TRUE, FALSE, 0);

  data->top_vbox = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (data->top_vbox), data->top_hbox, TRUE, FALSE, 0);

  data->tooltipbox = gtk_event_box_new ();
  gtk_container_add (GTK_CONTAINER (data->tooltipbox), data->top_vbox);
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (data->tooltipbox), FALSE);
  gtk_widget_show_all (data->tooltipbox);

#if GTK_CHECK_VERSION(2,12,0)
  g_object_set (G_OBJECT(data->tooltipbox), "has-tooltip", TRUE, NULL);
  g_signal_connect(G_OBJECT(data->tooltipbox), "query-tooltip",
		   G_CALLBACK(weather_get_tooltip_cb),
		  data);
#endif
  xfce_panel_plugin_add_action_widget (plugin, data->tooltipbox);

  g_signal_connect (G_OBJECT (data->tooltipbox), "button-press-event",
                    G_CALLBACK (cb_click), data);
  g_signal_connect (G_OBJECT (data->tooltipbox), "scroll-event",
                    G_CALLBACK (cb_scroll), data);
  gtk_widget_add_events(data->scrollbox, GDK_BUTTON_PRESS_MASK);

  /* add refresh button to right click menu, for people who missed the middle mouse click feature */
  refresh = gtk_image_menu_item_new_from_stock ("gtk-refresh", NULL);
  gtk_widget_show (refresh);

  g_signal_connect (G_OBJECT (refresh), "activate",
                    G_CALLBACK (mi_click), data);

  xfce_panel_plugin_menu_insert_item (plugin, GTK_MENU_ITEM (refresh));

  /* add refresh button to right click menu, for people who missed the middle mouse click feature */
  mi = gtk_image_menu_item_new_with_mnemonic (_("_Forecast"));
  icon = get_icon ("SUN", 16, FALSE);
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(mi),
  	gtk_image_new_from_pixbuf(icon));
  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));
  gtk_widget_show (mi);

  g_signal_connect (G_OBJECT (mi), "activate",
                    G_CALLBACK (forecast_click), data);

  xfce_panel_plugin_menu_insert_item (plugin, GTK_MENU_ITEM (mi));

  /* assign to tempval because g_array_append_val () is using & operator */
  lbl = TEMPERATURE;
  g_array_append_val (data->labels, lbl);
  lbl = WIND_DIRECTION;
  g_array_append_val (data->labels, lbl);
  lbl = WIND_SPEED;
  g_array_append_val (data->labels, lbl);

  /* FIXME Without this the first label looks odd, because
   * the gc isn't created yet */
  gtk_scrollbox_set_label (GTK_SCROLLBOX (data->scrollbox), -1, "1");
  gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));

  data->updatetimeout =
    g_timeout_add_seconds (UPDATE_TIME, (GSourceFunc) update_weatherdata, data);

  return data;
}



static void
xfceweather_free (XfcePanelPlugin  *plugin,
                  xfceweather_data *data)
{
  weather_http_cleanup_qeue ();

  if (data->weatherdata)
    xml_weather_free (data->weatherdata);

  if (data->updatetimeout)
    {
      g_source_remove (data->updatetimeout);
      data->updatetimeout = 0;
    }

  xmlCleanupParser ();

  /* Free Tooltip */
#if !GTK_CHECK_VERSION(2,12,0)
  gtk_tooltips_set_tip (data->tooltips, data->tooltipbox, NULL, NULL);
  g_object_unref (G_OBJECT (data->tooltips));
#endif

  /* Free chars */
  g_free (data->lat);
  g_free (data->lon);
  g_free (data->location_name);
  g_free (data->proxy_host);

  /* Free Array */
  g_array_free (data->labels, TRUE);

  g_slice_free (xfceweather_data, data);
}



static gboolean
xfceweather_set_size (XfcePanelPlugin  *panel,
                      gint              size,
                      xfceweather_data *data)
{
  data->panel_size = size;

#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
  size /= xfce_panel_plugin_get_nrows (panel);
#endif

  data->size = size;

  gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));

  if (data->weatherdata)
    set_icon_current (data);
  else
    set_icon_error (data);

  /* we handled the size */
  return TRUE;
}



#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
static gboolean
xfceweather_set_mode (XfcePanelPlugin     *panel,
                      XfcePanelPluginMode  mode,
                      xfceweather_data    *data)
{
  GtkWidget *parent = gtk_widget_get_parent(data->vbox_center_scrollbox);

  data->panel_orientation = xfce_panel_plugin_get_mode(panel);
  data->orientation = (mode != XFCE_PANEL_PLUGIN_MODE_VERTICAL) ?
    GTK_ORIENTATION_HORIZONTAL : GTK_ORIENTATION_VERTICAL;

  g_object_ref(G_OBJECT(data->vbox_center_scrollbox));
  gtk_container_remove(GTK_CONTAINER(parent), data->vbox_center_scrollbox);

  if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
    gtk_box_pack_start (GTK_BOX (data->top_hbox), data->vbox_center_scrollbox, TRUE, FALSE, 0);
  else
    gtk_box_pack_start (GTK_BOX (data->top_vbox), data->vbox_center_scrollbox, TRUE, FALSE, 0);
  g_object_unref(G_OBJECT(data->vbox_center_scrollbox));

  if (data->panel_orientation == XFCE_PANEL_PLUGIN_MODE_DESKBAR)
    xfce_panel_plugin_set_small (XFCE_PANEL_PLUGIN (panel), FALSE);
  else
    xfce_panel_plugin_set_small (XFCE_PANEL_PLUGIN (panel), TRUE);

  gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));
  gtk_scrollbox_set_orientation (GTK_SCROLLBOX (data->scrollbox), data->orientation);

  if (data->weatherdata)
    set_icon_current (data);
  else
    set_icon_error (data);

  /* we handled the orientation */
  return TRUE;
}


#else
static gboolean
xfceweather_set_orientation (XfcePanelPlugin  *panel,
                             GtkOrientation    orientation,
                             xfceweather_data *data)
{
  GtkWidget *parent = gtk_widget_get_parent(data->vbox_center_scrollbox);

  data->orientation = GTK_ORIENTATION_HORIZONTAL;
  data->panel_orientation = orientation;

  g_object_ref(G_OBJECT(data->vbox_center_scrollbox));
  gtk_container_remove(GTK_CONTAINER(parent), data->vbox_center_scrollbox);

  if (data->panel_orientation == GTK_ORIENTATION_HORIZONTAL) {
    gtk_box_pack_start (GTK_BOX (data->top_hbox), data->vbox_center_scrollbox, TRUE, FALSE, 0);
  } else {
    gtk_box_pack_start (GTK_BOX (data->top_vbox), data->vbox_center_scrollbox, TRUE, FALSE, 0);
  }
  g_object_unref(G_OBJECT(data->vbox_center_scrollbox));

  gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));
  gtk_scrollbox_set_orientation (GTK_SCROLLBOX (data->scrollbox), data->panel_orientation);

  if (data->weatherdata)
    set_icon_current (data);
  else
    set_icon_error (data);

  /* we handled the orientation */
  return TRUE;
}
#endif


static void
weather_construct (XfcePanelPlugin *plugin)
{
  xfceweather_data *data;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  data = xfceweather_create_control (plugin);

  xfceweather_read_config (plugin, data);

  xfceweather_set_visibility (data);

#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
  xfceweather_set_mode (plugin, xfce_panel_plugin_get_mode(plugin), data);
#else
  xfceweather_set_orientation (plugin, xfce_panel_plugin_get_orientation(plugin), data);
#endif

  xfceweather_set_size (plugin, xfce_panel_plugin_get_size (plugin), data);

  gtk_container_add (GTK_CONTAINER (plugin), data->tooltipbox);

  g_signal_connect (G_OBJECT (plugin), "free-data",
                    G_CALLBACK (xfceweather_free), data);

  g_signal_connect (G_OBJECT (plugin), "save",
                    G_CALLBACK (xfceweather_write_config), data);

  g_signal_connect (G_OBJECT (plugin), "size-changed",
                    G_CALLBACK (xfceweather_set_size), data);

#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
  g_signal_connect (G_OBJECT (plugin), "mode-changed",
                    G_CALLBACK (xfceweather_set_mode), data);
#else
  g_signal_connect (G_OBJECT (plugin), "orientation-changed",
                    G_CALLBACK (xfceweather_set_orientation), data);
#endif

  xfce_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (G_OBJECT (plugin), "configure-plugin",
                    G_CALLBACK (xfceweather_create_options), data);

  update_weatherdata (data);
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (weather_construct);
