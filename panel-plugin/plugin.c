#include "plugin.h"
#include <sys/stat.h>
#include "debug_print.h"
#include <libxfce4util/i18n.h>
#include <libxfce4util/util.h>
#include <time.h>


gint IconSizeSmall = 0;

gchar *make_label(struct xml_weather *weatherdata, enum datas opt, enum units unit, gint size)
{
        gchar *str, *lbl, *txtsize;

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
                default: lbl = "?"; break;
        }

        switch (size)
        {
                case 2: txtsize = "medium"; break;
                case 3: txtsize = "medium"; break;
                default: txtsize="x-small"; break;
        }

       

        str = g_strdup_printf("<span size=\"%s\">%s: %s %s</span>", txtsize,
                        lbl, get_data(weatherdata, opt), get_unit(unit, opt));

        return str;
}
/* -1 error 0 no update needed 1 success */
gint update_weatherdata(struct xfceweather_data *data, gboolean force)
{
        xmlNode *cur_node = NULL;
        xmlDoc *doc = NULL;
        struct xml_weather *weather = NULL;
        gboolean ret;
        struct stat attrs; 
        /*gchar *fullfilename = xfce_resource_save_location(XFCE_RESOURCE_CACHE, 
                        filename, TRUE);*/
        gchar *fullfilename, *filename;

        if (!data->location_code)
                return -1;

        filename = g_strdup_printf("weather_%s_%c.xml", 
                        data->location_code, data->unit == METRIC ? 'm' : 'i');

        fullfilename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s", xfce_get_userdir(), 
                        filename);
        g_free(filename);

        if (!fullfilename)
        {
               
                return -1;
        } 

        if (force || (stat(fullfilename, &attrs) == -1) || 
                        ((time(NULL) - attrs.st_mtime) > (UPDATE_TIME)))
        {
                gchar *url = g_strdup_printf("/weather/local/%s?cc=*&dayf=%d&unit=%c",
                                data->location_code, XML_WEATHER_DAYF_N,
                                data->unit == METRIC ? 'm' : 'i');
               

                ret = http_get_file(url, "xoap.weather.com", fullfilename, 
                                data->proxy_host, data->proxy_port);
                g_free(url);

                if (!ret)
                {
                        g_free(fullfilename);
                       
                        return -1;
                }
        }
        else if (data->weatherdata)
                return 0;
      

        doc = xmlParseFile(fullfilename);
        g_free(fullfilename);

        if (!doc)
                return -1;

       

        cur_node = xmlDocGetRootElement(doc);

        if (cur_node)
                weather = parse_weather(cur_node);

        xmlFreeDoc(doc);

        if (weather)
        {
               
                if (data->weatherdata)
                        xml_weather_free(data->weatherdata);
               
                data->weatherdata = weather;
                return 1;
        }
        else
                return -1;
}       

void update_plugin (struct xfceweather_data *data, gboolean force)
{
        int i;
        GdkPixbuf *icon = NULL; 
       
        gtk_scrollbox_clear(GTK_SCROLLBOX(data->scrollbox));
        
        if (update_weatherdata(data, force) == -1)
        {
                GdkPixbuf *icon = get_icon(data->iconimage, "25", data->iconsize);
                gtk_image_set_from_pixbuf(GTK_IMAGE(data->iconimage), icon);

                if (data->weatherdata)
                {
                        xml_weather_free(data->weatherdata);
                        data->weatherdata = NULL;
                }

                add_tooltip(data->tooltipbox, "Cannot update weather data");
                
                return;
        }

       

       

        for (i = 0; i < data->labels->len; i++)
        {
                enum datas opt;
                gchar *str;

                opt = g_array_index(data->labels, enum datas, i);
               
                str = make_label(data->weatherdata, opt, data->unit,
                                data->size);

               

                gtk_scrollbox_set_label(GTK_SCROLLBOX(data->scrollbox), -1, 
                                str);
                g_free(str);

               
        }

        

        gtk_scrollbox_enablecb(GTK_SCROLLBOX(data->scrollbox), TRUE);

       
                
        icon = get_icon(data->iconimage, get_data(data->weatherdata, WICON), data->iconsize);
       
        gtk_image_set_from_pixbuf(GTK_IMAGE(data->iconimage), icon);
       
        add_tooltip(data->tooltipbox, get_data(data->weatherdata, TRANS));
}

GArray *labels_clear (GArray *array)
{       
        if (!array || array->len > 0)
        {
                if (array)
                        g_array_free(array, TRUE);
                
                array = g_array_new(FALSE, TRUE, sizeof(enum datas));
        }

        return array;
}

void xfceweather_read_config (Control *control, xmlNodePtr node)
{
        gchar *value;
        struct xfceweather_data *data = (struct xfceweather_data *)control->data;

       

        if (!node || !node->children)
                return;

        node = node->children;

        if (!xmlStrEqual (node->name, (const xmlChar *) XFCEWEATHER_ROOT))
                return;

        value = xmlGetProp (node, (const xmlChar *) "loc_code");
        

        if (value) 
        {
                if (data->location_code)
                        g_free(data->location_code);
                
                data->location_code = g_strdup(value);
                g_free(value); 
        }

        value = xmlGetProp (node, (const xmlChar *) "celsius");

        if (value) 
        {
                if (atoi(value) == 1)
                        data->unit = IMPERIAL;
                else
                        data->unit = METRIC;
                g_free(value);
        }
        
        if (data->proxy_host)
        {
                g_free(data->proxy_host);
                data->proxy_host = NULL;
        }

        value = xmlGetProp (node, (const xmlChar *) "proxy_host");

        if (value)
        {
                data->proxy_host = g_strdup(value);
                g_free(value);
        }

        value = xmlGetProp (node, (const xmlChar *) "proxy_port");

        if (value)
        {
                data->proxy_port = atoi(value);
                g_free(value);
        }


        data->labels = labels_clear(data->labels); 
        
        for (node = node->children; node; node = node->next) {
                if (node->type != XML_ELEMENT_NODE)
                        continue;

                if (NODE_IS_TYPE(node, "label_")) { /* label_ because the values of the
                                                     previous plugin are invalid in this version * */
                        value = DATA(node);
                        if (value) {
                                gint val = atoi(value); 
                                g_array_append_val(data->labels, val);
                                g_free(value);
                        }
                }
        }

       

        update_plugin(data, FALSE);
}

void xfceweather_write_config (Control *control,
                xmlNodePtr parent)
{
        xmlNodePtr root, node;
        gchar *value;
        int i = 0;

        struct xfceweather_data *data = (struct xfceweather_data *)control->data;

        root = xmlNewTextChild (parent, NULL, XFCEWEATHER_ROOT, NULL);

        value = g_strdup_printf("%d", data->unit == IMPERIAL ? 1 : 0);
        xmlSetProp (root, "celsius", value);
        g_free(value);

        if (data->location_code)
                xmlSetProp (root, "loc_code", data->location_code);

        if (data->proxy_host)
        {
                xmlSetProp(root, "proxy_host", data->proxy_host);

                value = g_strdup_printf("%d", data->proxy_port);
                xmlSetProp(root, "proxy_port", value);
                g_free(value);
        }

        for (i = 0; i < data->labels->len; i++) 
        {
                enum datas opt = g_array_index (data->labels, enum datas, i);
                value = g_strdup_printf("%d", opt);

                node = xmlNewTextChild(root, NULL, "label_", value);
                g_free(value);
        }
}

gboolean update_cb(struct xfceweather_data *data)
{
        XFCE_PANEL_LOCK();
        
        update_plugin(data, FALSE);
        
        XFCE_PANEL_UNLOCK();
        
        return TRUE;
}

void real_update_config(struct xfceweather_data *data, gboolean force)
{
        if (data->updatetimeout)
                g_source_remove(data->updatetimeout);

        update_plugin(data, force);

        data->updatetimeout = gtk_timeout_add(UPDATE_TIME * 1000, (GSourceFunc)update_cb, data);
}


void update_config(struct xfceweather_data *data)
{
        real_update_config(data, FALSE);
}

void close_summary(GtkWidget *widget, gpointer *user_data)
{
        struct xfceweather_data *data = (struct xfceweather_data *)user_data;

       

        data->summary_window = NULL;
       
}

gboolean cb_click(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
        struct xfceweather_data *data = (struct xfceweather_data *)user_data;

        if (event->button == 1)
        {
               
                if (data->summary_window != NULL)
                {
                       
                        gtk_window_present(GTK_WINDOW(data->summary_window));
                }
                else
                {
                        data->summary_window = create_summary_window(data->weatherdata,
                                        data->unit);
                        g_signal_connect(data->summary_window, "destroy",
                                        G_CALLBACK(close_summary), (gpointer)data);
                                        
                        gtk_widget_show_all(data->summary_window);
                }
        }
        else if (event->button == 2)
                real_update_config(data, TRUE);

        return FALSE;
}


void xfceweather_create_options(Control *control, GtkContainer *container,
                GtkWidget *done)
{
        struct xfceweather_data *data = (struct xfceweather_data *)control->data;
        struct xfceweather_dialog *dialog = create_config_dialog(data, container, done);

        set_callback_config_dialog(dialog, update_config);
}
       

gboolean xfceweather_create_control(Control *control)
{
        struct xfceweather_data *data = g_new0(struct xfceweather_data, 1);
        gchar *path;
        GtkWidget *vbox, *vbox2;
        enum datas lbl;

        if (!IconSizeSmall)
                IconSizeSmall = gtk_icon_size_register("iconsize_small", 20, 20);

        path = g_strdup_printf("%s%s%s%s", THEMESDIR, G_DIR_SEPARATOR_S,
                        DEFAULT_W_THEME, G_DIR_SEPARATOR_S);
        register_icons(path);
        g_free(path);

        data->scrollbox = gtk_scrollbox_new();
       
        data->iconimage = gtk_image_new_from_pixbuf(get_icon(control->base, "-", IconSizeSmall));
        gtk_misc_set_alignment(GTK_MISC(data->iconimage), 0.5, 1);
       

        data->labels = g_array_new(FALSE, TRUE, sizeof(enum datas));
       
       
        vbox = gtk_vbox_new(FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), data->iconimage, TRUE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), data->scrollbox, TRUE, TRUE, 0); 

        data->tooltipbox = gtk_event_box_new();
        gtk_container_add(GTK_CONTAINER(data->tooltipbox), vbox);
        
        vbox2 = gtk_vbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox2), data->tooltipbox, FALSE, FALSE, 0);

        gtk_container_add(GTK_CONTAINER(control->base), vbox2);
        g_signal_connect(data->tooltipbox, "button-press-event", G_CALLBACK(cb_click), (gpointer)data);
        
        gtk_widget_show_all(vbox2);

        lbl = FLIK;
        g_array_append_val(data->labels, lbl);
        lbl = TEMP;
        g_array_append_val(data->labels, lbl);

        control->data = data;
        control->with_popup = FALSE;

        gtk_scrollbox_set_label(GTK_SCROLLBOX(data->scrollbox), -1, "1");
        gtk_scrollbox_clear(GTK_SCROLLBOX(data->scrollbox));

        data->updatetimeout = gtk_timeout_add(UPDATE_TIME * 1000, (GSourceFunc)update_cb, data);    

        return TRUE;
}

void xfceweather_free(Control *control)
{
        struct xfceweather_data *data = (struct xfceweather_data *)control->data;

        if (data->weatherdata)
                xml_weather_free(data->weatherdata);

        unregister_icons();

        g_free(data->location_code);
/*      XXX the buffer is shared amoung all instances of the plugin, 
 *      so it causes an segv when freeing it */
/*        free_get_data_buffer();  */
        
        g_array_free(data->labels, TRUE);

        xmlCleanupParser();
}

void xfceweather_attach_callback (Control *control, const gchar *signal, GCallback cb,
                gpointer data)
{
        struct xfceweather_data *datae = (struct xfceweather_data*) control->data;

        g_signal_connect(datae->tooltipbox, signal, cb, data);
}

void xfceweather_set_size(Control *control, gint size)
{
        struct xfceweather_data *data = (struct xfceweather_data *)control->data;

        data->size = size;

        switch (data->size)
        {
                case 0: data->iconsize = IconSizeSmall; break;
                case 1: data->iconsize = GTK_ICON_SIZE_LARGE_TOOLBAR; break;
                case 2: data->iconsize = GTK_ICON_SIZE_DND; break;
                case 3: data->iconsize = GTK_ICON_SIZE_DIALOG; break; 
        }

        update_plugin(data, FALSE);
}

G_MODULE_EXPORT void
xfce_control_class_init (ControlClass * cc)
{
        xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

        cc->name = "weather";
        cc->caption = _("Weather update");

        cc->create_control = (CreateControlFunc) xfceweather_create_control;
        cc->attach_callback = xfceweather_attach_callback;
        cc->read_config = xfceweather_read_config;
        cc->write_config = xfceweather_write_config;

        cc->create_options = xfceweather_create_options;
        cc->free = xfceweather_free;
        cc->set_size = xfceweather_set_size;
}

XFCE_PLUGIN_CHECK_INIT
