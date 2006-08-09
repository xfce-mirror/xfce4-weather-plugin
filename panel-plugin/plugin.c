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

#include <string.h>
#include <sys/stat.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "parsers.h"
#include "get_data.h"
#include "plugin.h"

#include "translate.h"
#include "http_client.h"
#include "summary_window.h"
#include "config_dialog.h"
#include "icon.h"
#include "scrollbox.h"

#define XFCEWEATHER_ROOT "weather"
#define UPDATE_TIME 1600
#define BORDER 8

gboolean
check_envproxy (gchar **proxy_host,
                gint   *proxy_port)
{

    gchar *env_proxy = getenv ("HTTP_PROXY"), *tmp, **split;

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
    *proxy_port = (int)strtol (split[1], NULL, 0);

    g_strfreev (split);

    return TRUE;
}

static gint
GTK_ICON_SIZE_SMALL = 0;

static gchar *
make_label (xml_weather *weatherdata,
            datas        opt,
            units        unit,
            gint         size)
{

    gchar *str, *value;
    const gchar *rawvalue, *lbl, *txtsize;

    switch (opt)
    {
        case VIS:       lbl = _("V"); break;
        case UV_INDEX:  lbl = _("U"); break;
        case WIND_DIRECTION: lbl = _("WD"); break;
        case BAR_D:     lbl = _("P"); break;
        case BAR_R:     lbl = _("P"); break;
        case FLIK:      lbl = _("F"); break;
        case TEMP:      lbl = _("T"); break;
        case DEWP:      lbl = _("D"); break;
        case HMID:      lbl = _("H"); break;
        case WIND_SPEED:lbl = _("WS"); break;
        case WIND_GUST: lbl = _("WG"); break;
        default:        lbl = "?"; break;
    }

    /* arbitrary, choose something that works */
    if (size > 36)
        txtsize = "medium";
    else if (size > 30)
        txtsize = "small";
    else if (size > 24)     
        txtsize = "x-small";
    else
        txtsize = "xx-small";

    rawvalue = get_data(weatherdata, opt);

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
    
    return str;
}

static gchar *
get_filename (const xfceweather_data *data)
{

    gchar *filename = 
        g_strdup_printf ("xfce4/weather-plugin/weather_%s_%c.xml", 
            data->location_code, data->unit == METRIC ? 'm' : 'i');

    gchar *fullfilename = 
        xfce_resource_save_location (XFCE_RESOURCE_CACHE, filename, 
                        TRUE);
    g_free (filename);

    return fullfilename;
}

static void
set_icon_error (xfceweather_data *data)
{
    GdkPixbuf *icon = get_icon ("25", data->iconsize);
    gtk_image_set_from_pixbuf (GTK_IMAGE(data->iconimage), icon);
    g_object_unref (G_OBJECT (icon));

    if (data->weatherdata)
    {
        xml_weather_free (data->weatherdata);
        data->weatherdata = NULL;
    }

    gtk_tooltips_set_tip (data->tooltips, data->tooltipbox, 
                 _("Cannot update weather data"), NULL);

    return;
}

static void
set_icon_current (xfceweather_data *data)
{
    guint      i;
    GdkPixbuf *icon = NULL;

    for (i = 0; i < data->labels->len; i++)
    {
        datas opt;
        gchar *str;

        opt = g_array_index(data->labels, datas, i);
           
        str = make_label (data->weatherdata, opt, data->unit,
                data->size);

        gtk_scrollbox_set_label (GTK_SCROLLBOX(data->scrollbox), -1, 
                str);
	
        g_free (str);
    }

    gtk_scrollbox_enablecb(GTK_SCROLLBOX(data->scrollbox), TRUE);
    
    icon = get_icon (get_data(data->weatherdata, WICON), data->iconsize);
    gtk_image_set_from_pixbuf (GTK_IMAGE(data->iconimage), icon);
    g_object_unref (G_OBJECT (icon));
       
    gtk_tooltips_set_tip (data->tooltips, data->tooltipbox, 
        translate_desc(get_data(data->weatherdata, TRANS)), NULL);
}

static void
cb_update (gboolean status,
           gpointer user_data)
{


    xfceweather_data *data = (xfceweather_data *)user_data;
    gchar            *fullfilename = get_filename (data);
    xmlDoc           *doc;
    xmlNode          *cur_node;
    xml_weather      *weather = NULL;

    if (!fullfilename)
        return;
    
    doc = xmlParseFile (fullfilename);
    g_free (fullfilename);

    if (!doc)
        return;

    cur_node = xmlDocGetRootElement (doc);
    
    if (cur_node)
        weather = parse_weather (cur_node);
    
    xmlFreeDoc (doc);

    gtk_scrollbox_clear (GTK_SCROLLBOX(data->scrollbox));

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
        return;
    }
}

/* -1 error 0 no update needed 1 updating */
static gint
update_weatherdata (xfceweather_data *data,
                    gboolean          force)
{

    struct stat attrs; 
    gchar *fullfilename;
    gchar *url;

    if (!data->location_code)
        return -1;

    fullfilename = get_filename (data);

    if (!fullfilename)
    {
        DBG ("can't get savedir?");
        return -1;
    } 

    if (force || (stat (fullfilename, &attrs) == -1) || 
            ((time (NULL) - attrs.st_mtime) > (UPDATE_TIME)))
    {
        url = g_strdup_printf ("/weather/local/%s?cc=*&dayf=%d&unit=%c",
                data->location_code, XML_WEATHER_DAYF_N,
                data->unit == METRIC ? 'm' : 'i');

        gboolean status = http_get_file (url, "xoap.weather.com", fullfilename, 
                data->proxy_host, data->proxy_port, cb_update,
                (gpointer) data);
	
        g_free (url);
        g_free (fullfilename);

        return status ? 1 : -1;
    }
    else if (data->weatherdata)
    {
	return 0;
    }
    else
    {
        cb_update (TRUE, data);
        return 1;
    }
}

static void
update_plugin (xfceweather_data *data,
               gboolean          force)
{

    if (update_weatherdata(data, force) == -1)
    {
        gtk_scrollbox_clear (GTK_SCROLLBOX(data->scrollbox));
        set_icon_error (data);
    }
    /* else update will be called through the callback in http_get_file () */
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
xfceweather_read_config (XfcePanelPlugin  *plugin,
                         xfceweather_data *data)
{

    gchar label[10];
    guint i;
    const gchar *value;
    gchar *file;
    XfceRc *rc;
    gint val;
    
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

    data->labels = labels_clear (data->labels); 
    
    for (i = 0; i < 100 /* arbitrary */; ++i)
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

    for (i = 0; i < data->labels->len; i++) 
    {
        g_snprintf (label, 10, "label%d", i);

        xfce_rc_write_int_entry (rc, label, (gint)g_array_index (data->labels, datas, i));
    }

    xfce_rc_close (rc);
}

static gboolean
update_cb (xfceweather_data *data)
{

    DBG ("update_cb(): callback called");
    
    update_plugin (data, FALSE);

    DBG ("update_cb(): request added, returning");
    
    return TRUE;
}

static void
update_plugin_with_reset (xfceweather_data *data,
                          gboolean          force)
{

    if (data->updatetimeout)
        g_source_remove (data->updatetimeout);

    update_plugin (data, force);

    data->updatetimeout = gtk_timeout_add (UPDATE_TIME * 1000, (GSourceFunc)update_cb, data);
}


static void
update_config (xfceweather_data *data)
{

    update_plugin_with_reset (data, TRUE); /* force because units could have changed */
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
               
            gtk_window_present (GTK_WINDOW(data->summary_window));
        }
        else
        {
            data->summary_window = create_summary_window (data->weatherdata,
                    data->unit);
            g_signal_connect (data->summary_window, "destroy",
                    G_CALLBACK (close_summary), (gpointer)data);
                    
            gtk_widget_show_all (data->summary_window);
        }
    }
    else if (event->button == 2)
        update_plugin_with_reset (data, TRUE);

    return FALSE;
}

static void
mi_click (GtkWidget *widget,
          gpointer   user_data)
{
    xfceweather_data *data = (xfceweather_data *) user_data;

    update_plugin_with_reset (data, TRUE);
}

static void
xfceweather_dialog_response (GtkWidget          *dlg,
                             gint                response, 
                             xfceweather_dialog *dialog)
{

    xfceweather_data *data = (xfceweather_data *) dialog->wd;
    
    apply_options (dialog);
    
    gtk_widget_destroy (dlg);
    gtk_list_store_clear (dialog->mdl_xmloption);
    panel_slice_free (xfceweather_dialog, dialog);
    
    xfce_panel_plugin_unblock_menu (data->plugin);
    xfceweather_write_config (data->plugin, data);
}

static void
xfceweather_create_options (XfcePanelPlugin  *plugin,
                            xfceweather_data *data)
{

    GtkWidget          *dlg, *vbox;
    xfceweather_dialog *dialog;

    xfce_panel_plugin_block_menu (plugin);

    dlg = xfce_titled_dialog_new_with_buttons (_("Weather Update"),
                          GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
                          GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
                          GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                          NULL);

    gtk_container_set_border_width (GTK_CONTAINER (dlg), 2);
    gtk_window_set_icon_name  (GTK_WINDOW (dlg), "xfce4-settings");

    vbox = gtk_vbox_new (FALSE, BORDER);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), BORDER - 2);
    gtk_widget_show (vbox);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox,
                TRUE, TRUE, 0);
    
    dialog = create_config_dialog (data, vbox);

    g_signal_connect (dlg, "response", G_CALLBACK (xfceweather_dialog_response),
              dialog);

    set_callback_config_dialog (dialog, update_config);

    gtk_widget_show (dlg);
}
       

static xfceweather_data *
xfceweather_create_control (XfcePanelPlugin *plugin)
{

    xfceweather_data *data = panel_slice_new0 (xfceweather_data);
    GtkWidget    *vbox, *refresh;
    datas         lbl;
    GdkPixbuf    *icon = NULL;

    if (!GTK_ICON_SIZE_SMALL)
        GTK_ICON_SIZE_SMALL = gtk_icon_size_register ("iconsize_small", 16, 16);

    data->plugin = plugin;

    data->tooltips = gtk_tooltips_new ();
    g_object_ref (data->tooltips);
    gtk_object_sink (GTK_OBJECT (data->tooltips));

    data->scrollbox = gtk_scrollbox_new ();
  
    icon = get_icon ("25", GTK_ICON_SIZE_SMALL);
    data->iconimage = gtk_image_new_from_pixbuf (icon);
    gtk_misc_set_alignment (GTK_MISC(data->iconimage), 0.5, 1);
    g_object_unref (G_OBJECT (icon));

    data->labels = g_array_new (FALSE, TRUE, sizeof (datas));
       
    vbox = gtk_vbox_new (FALSE, 0);

    gtk_box_pack_start (GTK_BOX(vbox), data->iconimage, TRUE, FALSE, 0);
    gtk_box_pack_start (GTK_BOX(vbox), data->scrollbox, TRUE, TRUE, 0); 

    data->tooltipbox = gtk_event_box_new ();
    gtk_container_add (GTK_CONTAINER(data->tooltipbox), vbox);
    gtk_widget_show_all (data->tooltipbox);
    
    xfce_panel_plugin_add_action_widget (plugin, data->tooltipbox);

    g_signal_connect (data->tooltipbox, "button-press-event", 
            G_CALLBACK (cb_click), (gpointer)data);
            
    /* add refresh button to right click menu, for people who missed the middle mouse click feature */
    refresh = gtk_image_menu_item_new_from_stock ("gtk-refresh", NULL);
    gtk_widget_show (refresh);
    
    g_signal_connect (refresh, "activate", 
            G_CALLBACK (mi_click), (gpointer) data);
    
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
    gtk_scrollbox_set_label (GTK_SCROLLBOX(data->scrollbox), -1, "1");
    gtk_scrollbox_clear (GTK_SCROLLBOX(data->scrollbox));

    data->updatetimeout = 
        gtk_timeout_add (UPDATE_TIME * 1000, (GSourceFunc)update_cb, data);    

    return data;
}

static void
xfceweather_free (XfcePanelPlugin  *plugin,
                  xfceweather_data *data)
{

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
    gtk_tooltips_set_tip (data->tooltips, data->tooltipbox, NULL, NULL);
    g_object_unref (G_OBJECT (data->tooltips));
    
    /* Free chars */
    g_free (data->location_code);
    g_free (data->proxy_host);
    
    /* Free Array */
    g_array_free (data->labels, TRUE);

    panel_slice_free (xfceweather_data, data);
}

static gboolean
xfceweather_set_size (XfcePanelPlugin  *panel,
                     int               size, 
                     xfceweather_data *data)
{

    data->size = size;

    /* arbitrary, choose something that works */
    if (size <= 30)
        data->iconsize = GTK_ICON_SIZE_SMALL;
    else if (size <= 36)
        data->iconsize = GTK_ICON_SIZE_BUTTON;
    else if (size <= 48)
        data->iconsize = GTK_ICON_SIZE_LARGE_TOOLBAR;
    else
        data->iconsize = GTK_ICON_SIZE_DND;

    gtk_scrollbox_clear (GTK_SCROLLBOX(data->scrollbox));
    
    if (data->weatherdata)
        set_icon_current (data);
    else
	set_icon_error (data);

    return TRUE;
}

static void
weather_construct (XfcePanelPlugin *plugin)
{
    xfceweather_data *data;

    xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    data = xfceweather_create_control (plugin);
    
    xfceweather_read_config (plugin, data);

    xfceweather_set_size (plugin, xfce_panel_plugin_get_size (plugin), 
                  data);
    
    gtk_container_add (GTK_CONTAINER (plugin), data->tooltipbox);
    
    g_signal_connect (plugin, "free-data", G_CALLBACK (xfceweather_free), 
              data);
    
    g_signal_connect (plugin, "save", 
              G_CALLBACK (xfceweather_write_config), data);
    
    g_signal_connect (plugin, "size-changed", 
              G_CALLBACK (xfceweather_set_size), data);

    xfce_panel_plugin_menu_show_configure (plugin);
    g_signal_connect (plugin, "configure-plugin", 
              G_CALLBACK (xfceweather_create_options), data);
	      
    update_plugin (data, TRUE);
}

XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (weather_construct);
