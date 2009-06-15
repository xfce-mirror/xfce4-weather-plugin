/*  $Id$
 *
 *  Copyright (c) 2003-2007 Xfce Development Team
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
#include <libxfcegui4/libxfcegui4.h>

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

  if (!tmp || strlen (tmp) < 3)
    return FALSE;

  env_proxy = tmp + 3;

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
make_label (xml_weather    *weatherdata,
            datas          opt,
            units          unit,
            gint           size,
	    GtkOrientation orientation,
	    gboolean       multiple_labels)
{

  gchar       *str, *value;
  const gchar *rawvalue, *lbl, *txtsize;

  switch (opt)
    {
    case VIS:
      lbl = _("V");
      break;
    case UV_INDEX:
      lbl = _("U");
      break;
    case WIND_DIRECTION:
      lbl = _("WD");
      break;
    case BAR_D:
      lbl = _("P");
      break;
    case BAR_R:
      lbl = _("P");
      break;
    case FLIK:
      lbl = _("F");
      break;
    case TEMP:
      lbl = _("T");
      break;
    case DEWP:
      lbl = _("D");
      break;
    case HMID:
      lbl = _("H");
      break;
    case WIND_SPEED:
      lbl = _("WS");
      break;
    case WIND_GUST:
      lbl = _("WG");
      break;
    default:
      lbl = "?";
      break;
    }

  /* arbitrary, choose something that works */
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    txtsize = "medium";
  else if (size > 36)
    txtsize = "medium";
  else if (size > 30)
    txtsize = "small";
  else if (size > 24)
    txtsize = "x-small";
  else
    txtsize = "xx-small";

  rawvalue = get_data (weatherdata, opt);

  switch (opt)
    {
    case VIS:
      value = translate_visibility (rawvalue, unit);
      break;
    case WIND_DIRECTION:
      value = translate_wind_direction (rawvalue);
      break;
    case WIND_SPEED:
    case WIND_GUST:
      value = translate_wind_speed (rawvalue, unit);
      break;
    default:
      value = NULL;
      break;
    }


  if (multiple_labels) {
    if (value != NULL)
      {
	str = g_strdup_printf ("<span size=\"%s\">%s: %s</span>",
                               txtsize, lbl, value);
	g_free (value);
      }
    else
      {
	str = g_strdup_printf ("<span size=\"%s\">%s: %s %s</span>",
                               txtsize, lbl, rawvalue, get_unit (unit, opt));
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
                               txtsize, rawvalue, get_unit (unit, opt));
      }
  }
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

  size = data->size;

  if (data->weatherdata)
    {
      xml_weather_free (data->weatherdata);
      data->weatherdata = NULL;
    }

  /* arbitrary, choose something that works */

  if (data->orientation == GTK_ORIENTATION_HORIZONTAL)
    txtsize = "medium";
  else if (size > 36)
    txtsize = "medium";
  else if (size > 30)
    txtsize = "small";
  else if (size > 24)
    txtsize = "x-small";
  else
    txtsize = "xx-small";

  str = g_strdup_printf ("<span size=\"%s\">No Data</span>", txtsize);
  gtk_scrollbox_set_label (GTK_SCROLLBOX (data->scrollbox), -1, str);
  g_free (str);

  gtk_scrollbox_enablecb  (GTK_SCROLLBOX (data->scrollbox), TRUE);

  gtk_widget_get_size_request (data->scrollbox, NULL, &height);

  if (data->orientation == GTK_ORIENTATION_VERTICAL)
    icon = get_icon ("99", data->size - height - 2);
  else
    icon = get_icon ("99", data->size);

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
  guint           i;
  GdkPixbuf      *icon = NULL;
  datas           opt;
  gchar          *str;
  gint            size, height;

  for (i = 0; i < data->labels->len; i++)
    {
      opt = g_array_index (data->labels, datas, i);

      str = make_label (data->weatherdata, opt, data->unit, data->size, data->orientation, (data->labels->len > 1));

      gtk_scrollbox_set_label (GTK_SCROLLBOX (data->scrollbox), -1, str);

      g_free (str);
    }

  gtk_scrollbox_enablecb (GTK_SCROLLBOX (data->scrollbox), TRUE);

  if (i == 0)
    {
      size = data->size;
    }
  else
    {
      gtk_widget_get_size_request (data->scrollbox, NULL, &height);
      if (data->orientation == GTK_ORIENTATION_VERTICAL)
        size = data->size - height - 2;
      else
        size = data->size;
    }

  icon = get_icon (get_data (data->weatherdata, WICON), size);

  gtk_image_set_from_pixbuf (GTK_IMAGE (data->iconimage), icon);

  if (G_LIKELY (icon))
    g_object_unref (G_OBJECT (icon));

#if !GTK_CHECK_VERSION(2,12,0)
  gtk_tooltips_set_tip (data->tooltips, data->tooltipbox,
                        translate_desc (get_data (data->weatherdata, TRANS)),
                        NULL);
#endif
}



static void
cb_update (gboolean  succeed,
           gchar    *result,
           gpointer  user_data)
{
  xfceweather_data *data = user_data;
  xmlDoc           *doc;
  xmlNode          *cur_node;
  xml_weather      *weather = NULL;

  if (G_LIKELY (succeed && result))
    {
      doc = xmlParseMemory (result, strlen (result));

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

  if (!data->location_code)
    {
      gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));
      set_icon_error (data);

      return TRUE;
    }

  /* build url */
  url = g_strdup_printf ("/weather/local/%s?cc=*&dayf=%d&unit=%c&link=xoap&prod=xoap&par=%s&key=%s",
                         data->location_code, XML_WEATHER_DAYF_N,
                         data->unit == METRIC ? 'm' : 'i',
			 PARTNER_ID, LICENSE_KEY);

  /* start receive thread */
  weather_http_receive_data ("xoap.weather.com", url, data->proxy_host,
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

  value = xfce_rc_read_entry (rc, "loc_code", NULL);

  if (value)
    {
      if (data->location_code)
        g_free (data->location_code);

      data->location_code = g_strdup (value);
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

  if (data->location_code)
    xfce_rc_write_entry (rc, "loc_code", data->location_code);

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
    gtk_timeout_add (UPDATE_TIME * 1000, (GSourceFunc) update_weatherdata, data);
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



static gboolean
cb_click (GtkWidget      *widget,
          GdkEventButton *event,
          gpointer        user_data)
{
  xfceweather_data *data = (xfceweather_data *) user_data;

  if (event->button == 1)
    {

      if (data->summary_window != NULL)
        {

          gtk_window_present (GTK_WINDOW (data->summary_window));
        }
      else
        {
          data->summary_window = create_summary_window (data->weatherdata,
                                                        data->unit);
          g_signal_connect (G_OBJECT (data->summary_window), "destroy",
                            G_CALLBACK (close_summary), data);

          gtk_widget_show_all (data->summary_window);
        }
    }
  else if (event->button == 2)
    {
      update_weatherdata_with_reset (data);
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
      panel_slice_free (xfceweather_dialog, dialog);

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
  gchar *markup_text;
  
  if (data->weatherdata == NULL) {
    gtk_tooltip_set_text (tooltip, _("Cannot update weather data"));
  } else {
    markup_text = g_markup_printf_escaped(
  	  "<b>%s</b>\n"
	  "%s", 
	  get_data (data->weatherdata, DNAM),
	  translate_desc (get_data (data->weatherdata, TRANS))
	  );
    gtk_tooltip_set_markup (tooltip, markup_text);
    g_free(markup_text);
  }
  icon = get_icon (get_data (data->weatherdata, WICON), 32);
  gtk_tooltip_set_icon (tooltip, icon);
  g_object_unref (G_OBJECT(icon));
  
  return TRUE;
}


static xfceweather_data *
xfceweather_create_control (XfcePanelPlugin *plugin)
{
  xfceweather_data *data = panel_slice_new0 (xfceweather_data);
  GtkWidget        *refresh;
  datas             lbl;
  GdkPixbuf        *icon = NULL;

  data->plugin = plugin;

#if !GTK_CHECK_VERSION(2,12,0)
  data->tooltips = gtk_tooltips_new ();
  g_object_ref (data->tooltips);
  gtk_object_sink (GTK_OBJECT (data->tooltips));
#endif
  data->scrollbox = gtk_scrollbox_new ();

  icon = get_icon ("99", 16);
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
  gtk_widget_show_all (data->tooltipbox);
  GTK_WIDGET_SET_FLAGS (GTK_WIDGET(data->tooltipbox), GTK_NO_WINDOW);

#if GTK_CHECK_VERSION(2,12,0)
  g_object_set (G_OBJECT(data->tooltipbox), "has-tooltip", TRUE, NULL);
  g_signal_connect(G_OBJECT(data->tooltipbox), "query-tooltip", 
		   G_CALLBACK(weather_get_tooltip_cb),
		  data);
#endif
  xfce_panel_plugin_add_action_widget (plugin, data->tooltipbox);

  g_signal_connect (G_OBJECT (data->tooltipbox), "button-press-event",
                    G_CALLBACK (cb_click), data);
  gtk_widget_add_events(data->scrollbox, GDK_BUTTON_PRESS_MASK);

  /* add refresh button to right click menu, for people who missed the middle mouse click feature */
  refresh = gtk_image_menu_item_new_from_stock ("gtk-refresh", NULL);
  gtk_widget_show (refresh);

  g_signal_connect (G_OBJECT (refresh), "activate",
                    G_CALLBACK (mi_click), data);

  xfce_panel_plugin_menu_insert_item (plugin, GTK_MENU_ITEM (refresh));

  /* assign to tempval because g_array_append_val () is using & operator */
  lbl = TEMP;
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
    gtk_timeout_add (UPDATE_TIME * 1000, (GSourceFunc) update_weatherdata, data);

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

  free_get_data_buffer ();
  xmlCleanupParser ();

  /* Free Tooltip */
#if !GTK_CHECK_VERSION(2,12,0)
  gtk_tooltips_set_tip (data->tooltips, data->tooltipbox, NULL, NULL);
  g_object_unref (G_OBJECT (data->tooltips));
#endif

  /* Free chars */
  g_free (data->location_code);
  g_free (data->proxy_host);

  /* Free Array */
  g_array_free (data->labels, TRUE);

  panel_slice_free (xfceweather_data, data);
}



static gboolean
xfceweather_set_size (XfcePanelPlugin  *panel,
                      gint              size,
                      xfceweather_data *data)
{
  data->size = size;

  gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));

  if (data->weatherdata)
    set_icon_current (data);
  else
    set_icon_error (data);

  /* we handled the size */
  return TRUE;
}

static gboolean
xfceweather_set_orientation (XfcePanelPlugin  *panel,
                             GtkOrientation    orientation,
                             xfceweather_data *data)
{
  GtkWidget *parent = gtk_widget_get_parent(data->vbox_center_scrollbox);

  data->orientation = orientation;

  g_object_ref(G_OBJECT(data->vbox_center_scrollbox));
  gtk_container_remove(GTK_CONTAINER(parent), data->vbox_center_scrollbox);

  if (data->orientation == GTK_ORIENTATION_HORIZONTAL) {
    gtk_box_pack_start (GTK_BOX (data->top_hbox), data->vbox_center_scrollbox, TRUE, FALSE, 0);
  } else {
    gtk_box_pack_start (GTK_BOX (data->top_vbox), data->vbox_center_scrollbox, TRUE, FALSE, 0);
  }
  g_object_unref(G_OBJECT(data->vbox_center_scrollbox));

  gtk_scrollbox_clear (GTK_SCROLLBOX (data->scrollbox));
  
  if (data->weatherdata)
    set_icon_current (data);
  else
    set_icon_error (data);

  /* we handled the orientation */
  return TRUE;
}



static void
weather_construct (XfcePanelPlugin *plugin)
{
  xfceweather_data *data;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  data = xfceweather_create_control (plugin);

  xfceweather_read_config (plugin, data);

  xfceweather_set_visibility (data);

  xfceweather_set_size (plugin, xfce_panel_plugin_get_size (plugin), data);
  xfceweather_set_orientation (plugin, xfce_panel_plugin_get_orientation(plugin), data);

  gtk_container_add (GTK_CONTAINER (plugin), data->tooltipbox);

  g_signal_connect (G_OBJECT (plugin), "free-data",
                    G_CALLBACK (xfceweather_free), data);

  g_signal_connect (G_OBJECT (plugin), "save",
                    G_CALLBACK (xfceweather_write_config), data);

  g_signal_connect (G_OBJECT (plugin), "size-changed",
                    G_CALLBACK (xfceweather_set_size), data);

  g_signal_connect (G_OBJECT (plugin), "orientation-changed",
                    G_CALLBACK (xfceweather_set_orientation), data);

  xfce_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (G_OBJECT (plugin), "configure-plugin",
                    G_CALLBACK (xfceweather_create_options), data);

  update_weatherdata (data);
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (weather_construct);
