/* 
 * Weather update plugin for the xfce4 panel written by Bob Schlärmann
 *
 * Contributions by:
 *
 * Eoin Coffey: added option to display temperature in tooltip or plugin window
 *
 * Iconset from Liquid Weather
 * Weather information from www.weather.com
 *
 * 
 * This software is public domain
 * 
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/i18n.h>
#include <libxfce4util/debug.h>
#include <libxfcegui4/xfce_iconbutton.h>
#include <libxfcegui4/xfce_framebox.h>
#include <libxfcegui4/dialogs.h>
#include <libxfce4util/util.h>

#include <panel/xfce.h>
#include <panel/plugins.h>
#include <gtk/gtk.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkdrawingarea.h>

#include <gdk/gdk.h>

#include <stdio.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>

#define DEFAULTTHEME "liquid"

#include <fcntl.h>
#include <sys/select.h>

#include <sys/stat.h>

#define DATA(node) xmlNodeListGetString(node->doc, node->children, 1)
#define NODE_IS_TYPE(node, type) xmlStrEqual (node->name, (const xmlChar *) type)

#define BUFFER_LEN 256
#define WEATHER_SOURCE "xoap.weather.com"
#define XFCEWEATHER_ROOT "weather"
#define BORDER 6

#define MAX_RETRYCOUNT 100
#define UPDATETIME 1800 * 1000 
/* half an hour */
#define LABEL_REFRESH 2000
#define LABEL_SPEED 25

#define XMLOPTIONS 11
#define DAYS_FORECAST 5
static int iconsize_small = 0;

typedef enum {
        IMPERIAL = 0,
        METRIC = 1
} unit;

static gchar *weather_xml = NULL;
static gboolean get_file(gchar *, gchar *, unit);
gboolean cb_click (GtkWidget *, GdkEventButton *, gpointer);

typedef struct {
        gchar *lsup;
        gchar *obst;
        gchar *flik;
        gchar *t;
        gchar *icon;
        gchar *tmp;
        
        gchar *bar_r;
        gchar *bar_d;
        
        gchar *wind_s;
        gchar *wind_gust;
        gchar *wind_d;
        gchar *wind_t;
        
        gchar *hmid;
        gchar *vis;
        
        gchar *uv_i;
        gchar *uv_t;

        gchar *dewp;
} xmldata;

typedef struct {
        gchar *icon;
        gchar *t;
        gchar *wind_s;
        gchar *wind_t;
        gchar *ppcp;
        gchar *hmid;
} xmldata_f_p;

typedef struct {
        gchar *day;
        gchar *date;

        gchar *hi;
        gchar *low;

        xmldata_f_p *part_d;
        xmldata_f_p *part_n;
} xmldata_f;

typedef enum {
        FLIK = 0,
        TMP = 1,
        BAR_R = 2,
        BAR_D = 3,
        WIND_S = 4,
        WIND_GUST = 5,
        WIND_T = 6,
        HMID = 7,
        VIS = 8,
        UV_I = 9,
        DEWP = 10
} xmloptions;

typedef struct {
        gchar *loc_code;
        unit unit;

        GtkWidget *drawable_label;
        GdkPixmap *drawable_pixmap;
        gint drawable_pixmap_i;
        gint drawable_offset;
        gint drawable_maxoffset;
        gint drawable_timeout;
        gint drawable_middlemax;
        gint drawable_middle;
        
        GtkWidget *image_condition;
        GtkWidget *box_tooltip;
        GtkWidget *windowdata_pointer;
        GtkWidget *box_widgets;
        GtkWidget *box_labels;

        gint size;
        
        gint timeout_id;
        gint set_scrollwin_cb_id;

        xmldata *xmldata;
        GPtrArray *xmldata_f;

        gboolean tooltip;

        GtkIconFactory *iconfactory;

        GArray *xmldata_options;
        GPtrArray *xmldata_pixmaps;

} xfceweather_data;

typedef struct {
        GtkWidget *dialog;
        GtkWidget *opt_unit;
        GtkWidget *txt_loc_code;

        GtkWidget *tooltip_yes;
        GtkWidget *tooltip_no;

        GtkWidget *opt_xmloption;
        GtkWidget *lst_xmloption;
        GtkListStore *mdl_xmloption;

        xfceweather_data *wd;

        gchar *xml_options[XMLOPTIONS + 1];

} xfceweather_dialog;

enum units {
        TEMPERATURE = 0,
        WINDSPEED = 1,
        PRESSURE = 2,
        WINDGUST = 3,
        HUMIDITY = 4
};

const gchar *get_unit(unit unit, enum units what)
{
        /* these don't get freed on plugin unload */
        static const gchar *unitdescr_i[5] = {"F&#176;", "mph", "in", 
                "mph", "%"}; /* wind gust measured in ? */
        static const gchar *unitdescr_m[5] = {"C&#176;", "kph", 
                "hPa", "kph", "%"};
        
        if (unit == IMPERIAL)
                return unitdescr_i[what];
        else
                return unitdescr_m[what];
}

GdkPixmap *draw_pixmap (GtkWidget *drawable, xmldata *xmldata, 
                xmloptions option, unit u_unit, gint size, gint *drawable_middlemax)
{
        gchar *label, *size_d, *r_label, *label_d;
        GdkPixmap *pixmap;
        gboolean useunit = FALSE;
        enum units unit;
        GdkWindow *rootwin;
        PangoLayout *pl;
        GdkGC *gc;
        int height, width;
        gboolean widthset = FALSE;

        /* Need to know the size of the widget and it's gc
         * 
         * TODO is there a better way to do this? */
        if (!GTK_WIDGET_REALIZED(drawable)) {
                return NULL;
        }
        
        /* First the label text
         * XXX This should be some kind of static array with structs */        
        switch (option) {
                case FLIK: label_d = "F"; unit = TEMPERATURE; label = xmldata->flik; useunit = TRUE; break;
                case TMP: label_d = "T"; label = xmldata->tmp; unit = TEMPERATURE; useunit = TRUE; break;
                case BAR_R: label_d = "P"; label = xmldata->bar_r; unit = PRESSURE; useunit = TRUE; break;
                case BAR_D: label_d = "P"; label = xmldata->bar_d; break;
                case WIND_S: label_d = "WS"; label = xmldata->wind_s; unit = WINDSPEED; useunit = TRUE; break;
                case WIND_GUST: label_d = "WG"; label = xmldata->wind_gust; unit = WINDSPEED; useunit = TRUE; break;
                case WIND_T: label_d = "WD"; label = xmldata->wind_t; break;
                case HMID: label_d ="H"; label = xmldata->hmid; unit = HUMIDITY; useunit = TRUE; break;
                case VIS: label_d = "V"; label = xmldata->vis; break;
                case UV_I: label_d = "UV"; label = xmldata->uv_i; break;
                case DEWP: label_d = "DP"; label = xmldata->dewp; unit = TEMPERATURE; useunit = TRUE; break;
                default: label = "invalid"; break;
        }

        switch (size) {
                case 0: size_d = "x-small"; break; 
                case 1: size_d = "x-small"; break; /* small and large create ugly fonts here */
                case 2: size_d = "medium"; break;
                case 3: size_d = "medium"; break;
        }

        if (useunit)
                r_label = g_strdup_printf("<span size=\"%s\">%s: %s %s</span>", 
                                size_d, label_d, label, get_unit(u_unit, unit));
        else
                r_label = g_strdup_printf("<span size=\"%s\">%s: %s</span>",
                                size_d, label_d, label);

        
        rootwin = gtk_widget_get_root_window(drawable);
        
        
        pl = gtk_widget_create_pango_layout(drawable, NULL);
        pango_layout_set_markup(pl, r_label, -1);
        gc = gdk_gc_new(GDK_DRAWABLE(rootwin));

        pango_layout_get_pixel_size(pl, &width, &height);

        if (width > drawable->requisition.width) {
                gtk_drawing_area_size(GTK_DRAWING_AREA(drawable), width, drawable->requisition.height);

                widthset = TRUE;
                
                *drawable_middlemax = width / 2;
        }
        if (height > drawable->requisition.height) {
                gtk_drawing_area_size(GTK_DRAWING_AREA(drawable), 
                                widthset ? width : drawable->requisition.width, height);
        }
        
        pixmap = gdk_pixmap_new(GDK_DRAWABLE(rootwin), width, 
                        height, -1);


        gdk_draw_rectangle(GDK_DRAWABLE(pixmap), drawable->style->bg_gc[GTK_WIDGET_STATE(drawable)],
                        TRUE, 0, 0, width, height);
        gdk_draw_layout(GDK_DRAWABLE(pixmap), gc, 0, 0, pl);
 
        g_free(r_label);
        g_free(pl);
        g_free(gc);

        return pixmap;

}

gboolean draw_pixmaps (xfceweather_data *data)
{
        GArray *xmldata_options = data->xmldata_options;
        GPtrArray *xmldata_pixmaps;
        xmloptions option;
        GdkPixmap *pixmap;
        int i;

        if (data->xmldata->t == NULL || xmldata_options == NULL) /* work around for segfault when called and 
                                        xmldata has not been filled */
                return FALSE;
        

        gtk_drawing_area_size(GTK_DRAWING_AREA(data->drawable_label),
                        0, 0);

        xmldata_pixmaps = g_ptr_array_new();

        for (i = 0; i < xmldata_options->len; i++) {
                option = g_array_index (data->xmldata_options, xmloptions, i);
                
                if ((pixmap = draw_pixmap(data->drawable_label, data->xmldata, 
                                option, data->unit, data->size, &data->drawable_middlemax)) == NULL) {
                        g_ptr_array_free(xmldata_pixmaps, TRUE);
                        return FALSE;
                }
                                

                g_ptr_array_add(xmldata_pixmaps, (gpointer) pixmap);
        }       

        if (data->xmldata_pixmaps)
                g_ptr_array_free(data->xmldata_pixmaps, TRUE);

        data->xmldata_pixmaps = xmldata_pixmaps;
       
        return TRUE;
}

gboolean start_draw_down (xfceweather_data *);
void start_draw_up (xfceweather_data *);

gboolean draw_up (xfceweather_data *data)
{
        GdkRectangle update_rect = {0,0, data->drawable_label->allocation.width, 
                data->drawable_label->allocation.height};

        if (data->drawable_offset == 0) {
                data->drawable_timeout = g_timeout_add(LABEL_REFRESH, (GSourceFunc)start_draw_down, data);
                return FALSE;
        }
        else
                data->drawable_offset++; 
        
        gtk_widget_draw(data->drawable_label,&update_rect);
        return TRUE;
}

gboolean draw_down (xfceweather_data *data)
{
        GdkRectangle update_rect = {0, 0, data->drawable_label->allocation.width, 
                data->drawable_label->allocation.height};

        if (data->drawable_offset == data->drawable_maxoffset) {
                data->drawable_timeout = 0;
                start_draw_up(data);
                return FALSE;
        }
        else
                data->drawable_offset--;
        
        gtk_widget_draw(data->drawable_label, &update_rect);
        return TRUE;
}

void start_draw_up(xfceweather_data *data) {
        gint width, height;
        
        /* if only one pixmap don't start the callback */
        if (!data->xmldata_pixmaps || data->xmldata_pixmaps->len == 0)
                return;
        
        if (data->xmldata_pixmaps->len == 1) {
                data->drawable_pixmap = (GdkPixmap *)g_ptr_array_index(data->xmldata_pixmaps, 0);
                data->drawable_offset = 0;

                GdkRectangle update_rect = {0, 0, data->drawable_label->allocation.width,
                        data->drawable_label->allocation.height};

                gtk_widget_draw(data->drawable_label, &update_rect);
                return;
        }

        if (data->drawable_pixmap_i >= data->xmldata_pixmaps->len)
                data->drawable_pixmap_i = 0;
        
        data->drawable_pixmap = (GdkPixmap *)g_ptr_array_index(data->xmldata_pixmaps, data->drawable_pixmap_i++);

        gdk_drawable_get_size(GDK_DRAWABLE(data->drawable_pixmap), &width, &height);

        data->drawable_middle = data->drawable_middlemax - width / 2;
        
     
        data->drawable_timeout = g_timeout_add(LABEL_SPEED, (GSourceFunc)draw_up, data);
}
gboolean start_draw_down (xfceweather_data *data)
{
        data->drawable_timeout = g_timeout_add(LABEL_SPEED, (GSourceFunc)draw_down, data);
        return FALSE;
}

void set_scrollwin(xfceweather_data *);

gboolean set_scrollwin_cb(GtkWidget *widget, gpointer user_data)
{
        xfceweather_data *data = (xfceweather_data *)user_data;
        g_signal_handler_disconnect(widget, data->set_scrollwin_cb_id);
        data->set_scrollwin_cb_id = 0;
        set_scrollwin(data);
        

        return FALSE;
}

void set_scrollwin (xfceweather_data *data)
{
        if (!GTK_WIDGET_REALIZED(data->drawable_label)) {
        
                if (!data->set_scrollwin_cb_id)
                        data->set_scrollwin_cb_id = g_signal_connect(data->drawable_label, "realize",
                                        G_CALLBACK(set_scrollwin_cb), (gpointer)data);
       
                return;
        }

        if (data->drawable_timeout)
                g_source_remove(data->drawable_timeout);
        draw_pixmaps(data);
        data->drawable_maxoffset = -data->drawable_label->requisition.height;
        start_draw_up(data); 
}

gboolean drawable_expose_event (GtkWidget *widget, GdkEventExpose *event, gpointer rdata)
{
        xfceweather_data *data = (xfceweather_data *)rdata;
        if (data->drawable_pixmap)
                gdk_draw_pixmap(widget->window,
                        widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                        data->drawable_pixmap,
                        0, data->drawable_offset,
                        data->drawable_middle, 0,
                        widget->allocation.width,widget->allocation.height);
        return FALSE;
}

void set_icon (xfceweather_data *data)
{
        gchar *value;
        GtkIconSize size;

        switch (data->size) {
                case 0: size = iconsize_small; break; /* SMALL_TOOLBAR is too small */
                case 1: size = GTK_ICON_SIZE_LARGE_TOOLBAR; break; /* LARGE_TOOLBAR */
                case 2: size = GTK_ICON_SIZE_DND; break;
                case 3: size = GTK_ICON_SIZE_DIALOG; break;
        }

        if (data->xmldata->tmp == NULL)
                value = g_strdup_printf("xfceweather_36");
        else
                value = g_strdup_printf("xfceweather_%s", data->xmldata->icon);
        
        gtk_image_set_from_stock(GTK_IMAGE(data->image_condition), value, size);
        g_free(value);
}


/* xml parse functions */
void parse_forecast_part (xmlNode *cur_node, xmldata_f_p *xmldata_f_p)
{
        for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
                if (cur_node->type != XML_ELEMENT_NODE)
                        continue;



                if (NODE_IS_TYPE(cur_node, "icon"))
                        xmldata_f_p->icon = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "t"))
                        xmldata_f_p->t = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "wind")) {
                        xmlNode *child_node;

                        for (child_node = cur_node->children; child_node; 
                                        child_node = child_node->next) {
                                if (cur_node->type != XML_ELEMENT_NODE)
                                        continue;

                                if (NODE_IS_TYPE(child_node, "s"))
                                        xmldata_f_p->wind_s = DATA(child_node);
                                else if (NODE_IS_TYPE(child_node, "t"))
                                        xmldata_f_p->wind_t = DATA(child_node);
                        }
                }
                else if (NODE_IS_TYPE(cur_node, "ppcp"))
                        xmldata_f_p->ppcp = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "hmid"))
                        xmldata_f_p->hmid = DATA(cur_node);
        }
}

void parse_forecast (xmlNode *cur_node, xmldata_f *f)
{
        gchar *value;

        f->day = xmlGetProp (cur_node, (const xmlChar *) "t");
        f->date = xmlGetProp (cur_node, (const xmlChar *) "dt");

        for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
                if (cur_node->type != XML_ELEMENT_NODE)
                        continue;

                if (NODE_IS_TYPE(cur_node, "hi"))
                        f->hi = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "low"))
                        f->low = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "part")) {
                        xmldata_f_p *f_p = malloc(sizeof(xmldata_f_p));
                        memset(f_p, 0, sizeof(xmldata_f_p));
                        value = xmlGetProp (cur_node, (const xmlChar *) "p");

                        parse_forecast_part(cur_node, f_p);
                        
                        if (xmlStrEqual ((const xmlChar *)value, "d"))
                                f->part_d = f_p;
                        else if (xmlStrEqual ((const xmlChar *)value, "n"))
                                f->part_n = f_p;
                        g_free(value);
                }
        }
}
void parse_bar (xmlNode *cur_node, xmldata *xmldata)
{
        for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
                if (cur_node->type != XML_ELEMENT_NODE)
                        continue;
                
                if (NODE_IS_TYPE(cur_node, "r"))
                        xmldata->bar_r = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "d"))
                        xmldata->bar_d = DATA(cur_node);
        }
}

void parse_wind (xmlNode *cur_node, xmldata *xmldata)
{
        for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
                if (cur_node->type != XML_ELEMENT_NODE)
                        continue;

                if (NODE_IS_TYPE(cur_node, "s"))
                        xmldata->wind_s = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "gust"))
                        xmldata->wind_gust = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "d"))
                        xmldata->wind_d = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "t"))
                        xmldata->wind_t = DATA(cur_node);
        }
}

void parse_uv (xmlNode *cur_node, xmldata *xmldata)
{
        for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
                if (cur_node->type != XML_ELEMENT_NODE)
                        continue;

                if (NODE_IS_TYPE(cur_node, "i"))
                        xmldata->uv_i = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "t"))
                        xmldata->uv_t = DATA(cur_node);
        }
}

void parse_cc (xmlNode *cur_node, xmldata *xmldata)
{
        for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
                if (cur_node->type != XML_ELEMENT_NODE)
                        continue;

                if (NODE_IS_TYPE(cur_node, "tmp"))
                        xmldata->tmp = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "icon"))
                        xmldata->icon = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "t"))
                        xmldata->t = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "flik"))
                        xmldata->flik = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "bar"))
                        parse_bar(cur_node, xmldata);
                else if (NODE_IS_TYPE(cur_node, "wind"))
                        parse_wind(cur_node, xmldata);
                else if (NODE_IS_TYPE(cur_node, "hmid"))
                        xmldata->hmid = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "vis"))
                        xmldata->vis = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "uv"))
                        parse_uv(cur_node, xmldata);
                else if (NODE_IS_TYPE(cur_node, "dewp"))
                        xmldata->dewp = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "lsup"))
                        xmldata->lsup = DATA(cur_node);
                else if (NODE_IS_TYPE(cur_node, "obst"))
                        xmldata->obst = DATA(cur_node);
        }

}

void free_xmldata(xmldata *xmldata)
{
        g_free(xmldata->lsup);
        g_free(xmldata->obst);
        g_free(xmldata->flik);
        g_free(xmldata->t);
        g_free(xmldata->icon);
        g_free(xmldata->bar_r);
        g_free(xmldata->bar_d);
        g_free(xmldata->wind_s);
        g_free(xmldata->wind_gust);
        g_free(xmldata->wind_d);
        g_free(xmldata->wind_t);
        g_free(xmldata->hmid);
        g_free(xmldata->vis);
        g_free(xmldata->uv_i);
        g_free(xmldata->uv_t);
        g_free(xmldata->dewp);
        g_free(xmldata->tmp);
}

void free_xmldata_f_p(xmldata_f_p *p)
{
       g_free(p->icon);
       g_free(p->t);
       g_free(p->wind_s);
       g_free(p->wind_t);
       g_free(p->ppcp);
       g_free(p->hmid);
}

void free_xmldata_f(xmldata_f *f)
{
        g_free(f->day);
        g_free(f->date);
        g_free(f->hi);
        g_free(f->low);

        free_xmldata_f_p(f->part_n);
        free_xmldata_f_p(f->part_d);
}

void free_xmldata_f_array(GPtrArray *array)
{
        int i;

        for (i=0; i < array->len; i++) {
                xmldata_f *data = (xmldata_f*) g_ptr_array_index(array, i);

                free_xmldata_f(data);
        }

        g_ptr_array_free(array, TRUE);
}

               
static gboolean
update(xfceweather_data *data, gboolean force)
{
        //gchar *value, *text;
        //PangoAttrList *list;
        
        xmlNode *cur_node = NULL;
        xmlDoc *doc = NULL;

        gchar *filename;
        xmldata *xmldata = data->xmldata;

        struct stat attrs;


        if ((filename = g_strdup_printf("%s%s_%c%s", weather_xml, data->loc_code,
                                        data->unit == IMPERIAL ? 'i' : 'm', ".xml")) == NULL) {
                printf("Out of memory\n"); return TRUE;
        }
 
        if (force == TRUE || (stat(filename, &attrs) == -1) || 
                        (time(NULL) - attrs.st_mtime) >= (60 * 25)) { /* a little time margin because 
                                                                         of the way the callback works */
                if (!get_file(data->loc_code, filename, data->unit)) {
                        g_free(filename);
                        return TRUE;
                }
        }

        doc = xmlParseFile(filename);
        
        g_free(filename);

        if (doc == NULL) {
                printf("Error while parsing file\n");
                return TRUE;
        }

        if ((cur_node = xmlDocGetRootElement(doc)) == NULL) {
                printf("Empty document\n");
                return TRUE;
        }

        for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next) {
                if (cur_node->type != XML_ELEMENT_NODE)
                        continue;

                if (NODE_IS_TYPE(cur_node, "cc")) {
                        free_xmldata(xmldata);
                        parse_cc(cur_node, xmldata);
                }
                else if (NODE_IS_TYPE(cur_node, "dayf")) {
                        xmlNodePtr child_node;
                        
                        if (data->xmldata_f)
                                free_xmldata_f_array(data->xmldata_f);

                        data->xmldata_f = g_ptr_array_sized_new(DAYS_FORECAST);

                        for (child_node = cur_node->children; child_node; 
                                        child_node = child_node->next) {
                                if (cur_node->type != XML_ELEMENT_NODE)
                                        continue;

                                if (NODE_IS_TYPE(child_node, "day")) { 
                                        xmldata_f *f = malloc(sizeof(xmldata_f));
                                        memset(f, 0, sizeof(xmldata_f));
                                        
                                        parse_forecast(child_node, f);

                                        g_ptr_array_add(data->xmldata_f, f);
                                }
                        }
                }
        }

        xmlFreeDoc(doc);

        /*

        if (data->tooltip == FALSE) {
                set_temperature(data);

                gtk_widget_show(data->label_temperature);
                add_tooltip(data->box_tooltip, xmldata->t);
        }
        else {
                gtk_widget_hide(data->label_temperature);

                pango_parse_markup("&#176;", -1, 0, &list, &text, NULL, NULL);
                value = g_strdup_printf("%s: %s %s%s", xmldata->t, xmldata->tmp, text, 
                                get_unit(data->unit, TEMPERATURE));
                
                add_tooltip(data->box_tooltip, value);
                g_free(value);
        }

        */

        add_tooltip(data->box_tooltip, xmldata->t);

        set_scrollwin(data);
        set_icon(data);

        xmlCleanupParser();


        return TRUE;
}

static gboolean cb_update(xfceweather_data *data)
{
        return update(data, FALSE);
}

int find_partsep (gchar *buffer, int n)
{
        int i;
        
        for (i = 1; i < n; i++) {
                if (buffer[i - 1] == '\n' && buffer[i] == '\r') {
                        return i;
                }
        }

        return 0;
}

static gboolean
get_file(gchar *loc_code, gchar *filename, unit unit)
{
        int retry, bytes_total, bytes_left, len_request, fd=0;
        gchar *request, unit_specify = 'm';

        struct sockaddr_in dest_host;
        struct hostent *host_address;
        struct timeval tv;

        char buffer[BUFFER_LEN];

        fd_set writefds, readfds;

        FILE *file = NULL;

        int n, i;
        gboolean error = TRUE, header_found = FALSE, add_offset = FALSE;

        if ((host_address = gethostbyname(WEATHER_SOURCE)) == NULL)
                return FALSE;

        if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
                return FALSE;

        fcntl(fd, F_SETFL, O_NONBLOCK);
        
        dest_host.sin_family = AF_INET;
        dest_host.sin_addr = *((struct in_addr *)host_address->h_addr);
        dest_host.sin_port = htons(80);
        memset(&(dest_host.sin_zero), '\0', 8);

        connect(fd, (struct sockaddr *)&dest_host, sizeof(struct sockaddr));

        if (unit == IMPERIAL)
                unit_specify = 'i';  


        /* g_timeout_add(100, (GSourceFunc) cb_getfile) */
        request = g_strdup_printf("GET /weather/local/%s?cc=&prod=xoap&" 
                          "unit=%c&par=1003832479&key=bb12936706a2d601&dayf=%d HTTP/1.0\r\n"
                          "Host: %s\r\n\r\n", loc_code, unit_specify, DAYS_FORECAST, WEATHER_SOURCE);

        len_request = strlen(request);
        bytes_total = 0; bytes_left = len_request;

        retry = 0;

        while(bytes_total < len_request) {
                tv.tv_sec = 0;
                tv.tv_usec = 50000;
                
                FD_ZERO(&writefds);
                FD_SET(fd, &writefds);
 
                select(fd + 1, NULL, &writefds, NULL, &tv);

                if (FD_ISSET(fd, &writefds)) {
                        if ((n = send(fd, request + bytes_total, bytes_left, 0)) == -1 || n == 0) {
                                n = -1;
                                printf("Error while sending\n"); 
                                break;
                        }
                        bytes_total += n;
                        bytes_left -= n;
                }
                else {
                        if (retry++ > MAX_RETRYCOUNT) {
                                n = -1;
                                break;
                        }
                                
                }
                
                while (gtk_events_pending ())
                        gtk_main_iteration ();
        }

       g_free(request);

       if (n == -1) {
               return FALSE;
       }

       retry = 0;


       while(1) {
               tv.tv_sec = 0;
               tv.tv_usec = 50000;

               FD_ZERO(&readfds);
               FD_SET(fd, &readfds);

               select(fd + 1, &readfds, NULL, NULL, &tv);

               if (FD_ISSET(fd, &readfds)) {
                       if ((n = recv(fd, buffer, BUFFER_LEN, 0)) == -1 || n == 0) {
                               break;
                       }

                       if (header_found != TRUE && (i = find_partsep(buffer, n))) {
                               header_found = TRUE;
                               add_offset = TRUE;
                               file = fopen(filename, "w");

                               if (file == NULL) {
                                       printf("Not opened file\n");
                                       error = TRUE;
                                       close(fd);
                                       return FALSE;
                               }
                       }

                       if (header_found == TRUE) {
                               if (add_offset == TRUE) {
                                       fwrite(buffer + (i + 2), sizeof(char), n - (i + 2), file);
                                       add_offset = FALSE;
                               }
                               else
                                       fwrite(buffer, sizeof(char), n, file);
                       }
               }
               else {
                       if (retry++ > MAX_RETRYCOUNT)
                               return FALSE;
               }

               while (gtk_events_pending())
                       gtk_main_iteration();
       }

       if (file != NULL) {
               fclose(file);
       }
       close(fd);

       return error;
}

void
add_plugin_widgets (GtkWidget *vbox, xfceweather_data *data)
{ 
        data->image_condition = gtk_image_new_from_stock("xfceweather_36",
                        GTK_ICON_SIZE_LARGE_TOOLBAR);
        gtk_widget_show(data->image_condition);
        data->drawable_label = gtk_drawing_area_new(); 

        g_signal_connect (G_OBJECT (data->drawable_label), "expose_event",
                        G_CALLBACK (drawable_expose_event), data);



        gtk_box_pack_start(GTK_BOX(vbox), data->image_condition, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), data->drawable_label, FALSE, FALSE, 0);
}

GtkIconFactory *
init_icons (void)
{
        GdkPixbuf *pixbuf;
        GtkIconFactory *factory;
        gchar *filename, *name;
        gchar *directory = g_strdup_printf("%s%s%s%s", THEMESDIR, G_DIR_SEPARATOR_S, 
                        DEFAULTTHEME, G_DIR_SEPARATOR_S);
        GtkIconSet *iconset;
        int i;

        factory = gtk_icon_factory_new();

        for (i = 1; i <= 44; i++) {
                filename = g_strdup_printf("%s%d.png", directory, i);
                name = g_strdup_printf("%s%d", "xfceweather_", i);

                if ((pixbuf = gdk_pixbuf_new_from_file(filename, NULL)) == NULL) {
                        printf("Error loading %s\n", filename);
                        continue;
                }

                iconset = gtk_icon_set_new_from_pixbuf(pixbuf);

                gtk_icon_factory_add(factory, name, iconset);

                g_free(filename);
                g_free(name);
        }

        if (iconsize_small == 0)
                iconsize_small = gtk_icon_size_register("iconsize_small", 20, 20);

        g_free(directory);

        gtk_icon_factory_add_default(factory);

        return factory;
}

gboolean
xfceweather_create_control(Control *control)
{
        GtkWidget *vbox, *vbox2, *box;
        xmloptions option1 = HMID;
        xmloptions option2 = TMP;
        xmloptions option3 = FLIK;

        xfceweather_data *data = malloc(sizeof(xfceweather_data));
        memset(data, 0, sizeof(xfceweather_data));

        data->xmldata = malloc(sizeof(xmldata));
        memset(data->xmldata, 0, sizeof(xmldata)); 
        
        data->xmldata_options = g_array_new(FALSE, FALSE, sizeof(xmloptions));
        
        g_array_append_val(data->xmldata_options, option1);
        g_array_append_val(data->xmldata_options, option2);
        g_array_append_val(data->xmldata_options, option3);

        data->iconfactory = init_icons();

        vbox = gtk_vbox_new(FALSE, 0);
        vbox2 = gtk_vbox_new(FALSE, 0);
        
        box = gtk_event_box_new();

        gtk_container_add(GTK_CONTAINER(box), vbox);
        add_plugin_widgets(vbox,data);

        gtk_box_pack_start(GTK_BOX(vbox2), box, TRUE, FALSE, 0);
        /* This is to get it the box aligned in the middle */
        
        gtk_container_add(GTK_CONTAINER(control->base), vbox2);

        gtk_widget_show_all(control->base);

        data->box_tooltip = box;

        g_signal_connect(box, "button-press-event", G_CALLBACK(cb_click), (gpointer *)data);

        weather_xml = g_strdup_printf("%s%s%s", xfce_get_userdir(), G_DIR_SEPARATOR_S, "weather_");

        control->data = data;
        control->with_popup = FALSE;
        
        return TRUE;
}

void xfceweather_free(Control *control)
{
        xfceweather_data *data = (xfceweather_data *)control->data;

        free_xmldata(data->xmldata);

        if (data->xmldata_f)
                free_xmldata_f_array(data->xmldata_f);

        if (data->timeout_id)
                gtk_timeout_remove(data->timeout_id);

        if (data->drawable_timeout)
                g_source_remove(data->drawable_timeout);
        
        g_ptr_array_free(data->xmldata_pixmaps, TRUE);
        
        gtk_icon_factory_remove_default(data->iconfactory);
        
        
        g_free(weather_xml);
        g_free(data->loc_code);
        g_free(data);
}

void xfceweather_attach_callback (Control *control, const gchar *signal, GCallback cb,
                gpointer data)
{
        xfceweather_data *datae = (xfceweather_data*) control->data;

        g_signal_connect(datae->box_tooltip, signal, cb, data);
}

void xfceweather_apply_options (xfceweather_dialog *dialog)
{
        int history = 0;
        gboolean hasiter = FALSE;
        GtkTreeIter iter;

        xfceweather_data *data = (xfceweather_data *)dialog->wd;
        
        history = gtk_option_menu_get_history(GTK_OPTION_MENU(dialog->opt_unit));

        if (history == 0)
                data->unit = IMPERIAL;
        else 
                data->unit = METRIC;

        g_free(data->loc_code);

        data->loc_code = g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog->txt_loc_code)));

        /*

        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->tooltip_yes)) == TRUE)
                data->tooltip = TRUE;
        else
                data->tooltip = FALSE;
        */

        /*data->xmloption_1 = gtk_option_menu_get_history(GTK_OPTION_MENU(dialog->opt_xmloption_1));
        data->xmloption_2 = gtk_option_menu_get_history(GTK_OPTION_MENU(dialog->opt_xmloption_2));
        data->xmloption_3 = gtk_option_menu_get_history(GTK_OPTION_MENU(dialog->opt_xmloption_3));
*/

        if (data->xmldata_options)
                g_array_free(data->xmldata_options, TRUE);

        data->xmldata_options = g_array_new(FALSE, FALSE, sizeof(xmloptions));


        for (hasiter = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->mdl_xmloption), &iter);
                        hasiter == TRUE; hasiter = gtk_tree_model_iter_next(
                                GTK_TREE_MODEL(dialog->mdl_xmloption), &iter))
        {
                gint option;
                GValue value = {0, };

                gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->mdl_xmloption), &iter, 1, &value);
                option = g_value_get_int(&value);
                g_array_append_val(data->xmldata_options, option);
        }  

        if (data->timeout_id)
                gtk_timeout_remove(data->timeout_id);

        update(data, FALSE);

        data->timeout_id = gtk_timeout_add(UPDATETIME, (GtkFunction)cb_update, (gpointer) data);
}

void free_dialog (xfceweather_dialog *dialog)
{
        int i;
        gtk_widget_destroy(dialog->opt_unit);
        gtk_widget_destroy(dialog->txt_loc_code);

        for (i = 0; i < XMLOPTIONS; i++) {
                g_free(dialog->xml_options[i]);
        }

        g_free(dialog);
}

gboolean cb_helpbutton (GtkWidget *widget)
{
        GtkWidget *dialog, *label, *frame;

        dialog = gtk_dialog_new_with_buttons("Location code", NULL,
                        0, 
                        GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

        label = gtk_label_new("If you have a zip code enter the first 5 digits here.\n\n"
                        "If not, follow these steps to obtain your location code:\n\n"
                        "1. Go to http://www.weather.com.\n"
                "2. Do a search for the location you want data for.\n"
                "3. If you get a result: the location code resides in the link,\n"
                "it's the part between /local/ and the first ?");

        frame = xfce_framebox_new(_("Location code"), TRUE);

        xfce_framebox_add(XFCE_FRAMEBOX(frame), label);

        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), frame, FALSE, FALSE, 0);

        gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);

        gtk_widget_show_all(GTK_WIDGET(dialog));

        gtk_dialog_run(GTK_DIALOG(dialog));

        gtk_widget_destroy(GTK_WIDGET(dialog));
        
        return FALSE;
}

GtkWidget *xfceweather_create_options_xmloptions (xfceweather_dialog *dialog)
{
        GtkWidget *menu, *option;
        int i = 0;

        menu = gtk_menu_new();
        option = gtk_option_menu_new();

        for (i = 0; i < XMLOPTIONS; i++) {
                gtk_menu_append(GTK_MENU(menu), gtk_menu_item_new_with_label(dialog->xml_options[i]));
        }
                

        gtk_option_menu_set_menu(GTK_OPTION_MENU(option), menu);

        return option;
}

void add_mdl_xmloption(xfceweather_dialog *dialog, gint option)
{
        GtkTreeIter iter;

        if (!dialog->xml_options[option])
                return; 
        gtk_list_store_append(dialog->mdl_xmloption, &iter);
        gtk_list_store_set(dialog->mdl_xmloption, &iter,
                        0, dialog->xml_options[option],
                        1, option,
                        -1);
}

gboolean cb_addoption (GtkWidget *widget, gpointer *data_r)
{
        xfceweather_dialog *dialog = (xfceweather_dialog *)data_r;
        gint history = gtk_option_menu_get_history(GTK_OPTION_MENU(dialog->opt_xmloption));
        
        add_mdl_xmloption(dialog, history);

        return FALSE;
}

gboolean cb_deloption (GtkWidget *widget, gpointer *data_r)
{
        xfceweather_dialog *dialog = (xfceweather_dialog *)data_r;
        GtkTreeIter iter;
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->lst_xmloption));
        
        if (gtk_tree_selection_get_selected(selection, NULL, &iter))
                gtk_list_store_remove(GTK_LIST_STORE(dialog->mdl_xmloption), &iter);

        return FALSE;
}        

void xfceweather_create_options (Control *control, GtkContainer *container,
                GtkWidget *done)
{
        GtkWidget *vbox, *vbox2, *hbox, *label, *menu, *menu_item, *button_add, *button_del, *image, *button, *scroll;
        GtkSizeGroup *sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        GtkSizeGroup *sg_buttons = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        xfceweather_dialog *dialog = malloc(sizeof(xfceweather_dialog));
        GtkTreeViewColumn *column;
        GtkCellRenderer *renderer;
        gint i;
        gchar *xmloptions_string[] = { "Windchill (F)", "Temperature (T)", "Atmosphere pressure (P)", 
                "Atmosphere state (P)", "Wind speed (WS)", "Wind gust (WG)", "Wind direction (WD)",
                "Humidity (H)", "Visibility (V)", "UV Index (UV)", "Dewpoint (DP)", NULL };
 
        for (i = 0; i < XMLOPTIONS; i++) {
                dialog->xml_options[i] = g_strdup(xmloptions_string[i]);
        }

        dialog->wd = (xfceweather_data *)control->data;
        dialog->dialog = gtk_widget_get_toplevel(done);

        vbox = gtk_vbox_new(FALSE, BORDER);

        label = gtk_label_new("Measurement unit:");
        menu = gtk_menu_new();
        dialog->opt_unit = gtk_option_menu_new();

        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        
        gtk_menu_append(GTK_MENU(menu), gtk_menu_item_new_with_label("Imperial"));
        gtk_menu_append(GTK_MENU(menu), gtk_menu_item_new_with_label("Metric"));

        gtk_option_menu_set_menu(GTK_OPTION_MENU(dialog->opt_unit), menu);

        if (dialog->wd->unit == IMPERIAL)
                gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->opt_unit), 0);
        else
                gtk_option_menu_set_history(GTK_OPTION_MENU(dialog->opt_unit), 1);

        gtk_size_group_add_widget(sg, label);

        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->opt_unit, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);


        label = gtk_label_new("Location code:");
        dialog->txt_loc_code = gtk_entry_new();

        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        if (dialog->wd->loc_code != NULL)
                gtk_entry_set_text(GTK_ENTRY(dialog->txt_loc_code), dialog->wd->loc_code);
        gtk_size_group_add_widget(sg, label);

        button = gtk_button_new();
        image = gtk_image_new_from_stock(GTK_STOCK_HELP, GTK_ICON_SIZE_BUTTON);
        gtk_container_add(GTK_CONTAINER(button), image);
        g_signal_connect(button, "clicked", G_CALLBACK(cb_helpbutton), NULL);


        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->txt_loc_code, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

        /* Start list */

        dialog->opt_xmloption = xfceweather_create_options_xmloptions(dialog);
        dialog->mdl_xmloption = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
        dialog->lst_xmloption = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->mdl_xmloption));

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("Labels to display", renderer,
                        "text", 0);
        gtk_tree_view_append_column(GTK_TREE_VIEW(dialog->lst_xmloption), column);

        button_add = gtk_button_new_from_stock(GTK_STOCK_ADD);
        gtk_size_group_add_widget(sg_buttons, button_add);
        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->opt_xmloption, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), button_add, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

        button_del = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
        gtk_size_group_add_widget(sg_buttons, button_del);
        hbox = gtk_hbox_new(FALSE, BORDER);
        scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
                        GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scroll), dialog->lst_xmloption);
        gtk_box_pack_start(GTK_BOX(hbox), scroll, TRUE, TRUE, 0);
        
        vbox2 = gtk_vbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox2), button_del, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
        gtk_widget_set_size_request(dialog->lst_xmloption, -1, 120);


        if (dialog->wd->xmldata_options) {
                xmloptions option;
                GtkTreeIter iter;
                
                for (i = 0; i < dialog->wd->xmldata_options->len; i++) {
                        option = g_array_index (dialog->wd->xmldata_options, xmloptions, i);

                        add_mdl_xmloption(dialog, option);
                }
        }

        g_signal_connect(button_add, "clicked", G_CALLBACK(cb_addoption), dialog);
        g_signal_connect(button_del, "clicked", G_CALLBACK(cb_deloption), dialog);
        



/*      label = gtk_label_new("Display temperature in:");
        gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	
        dialog->tooltip_yes = gtk_radio_button_new_with_label(NULL, "tooltip");
        dialog->tooltip_no = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(dialog->tooltip_yes), "panel");
	
        if (dialog->wd->tooltip == TRUE) 
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->tooltip_yes), TRUE);
        else
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->tooltip_no), TRUE);

        gtk_size_group_add_widget(sg, label); 
        
        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->tooltip_yes, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->tooltip_no, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
*/
        
        
        g_signal_connect_swapped (done, "clicked",
                        G_CALLBACK (xfceweather_apply_options), dialog);

        g_signal_connect_swapped (dialog->dialog, "destroy",
                        G_CALLBACK (free_dialog), dialog);
        
        gtk_widget_show_all(vbox);

        gtk_container_add (container, vbox);
}

void xfceweather_read_config (Control *control, xmlNodePtr node)
{
        xmlChar *value;
        xfceweather_data *data = (xfceweather_data *)control->data;

        if (!node || !node->children)
                return;

        node = node->children;

        if (!xmlStrEqual (node->name, (const xmlChar *) XFCEWEATHER_ROOT))
                return;

        value = xmlGetProp (node, (const xmlChar *) "loc_code");

        g_free(data->loc_code);

        if (value) {
                data->loc_code = g_strdup(value);
                g_free(value);
        }
        else
                data->loc_code = "NLXX0019";

        value = xmlGetProp (node, (const xmlChar *) "celsius");

        if (value) {
                if (atoi(value) == 0)
                        data->unit = IMPERIAL;
                else
                        data->unit = METRIC;
                g_free(value);
        }

        value = xmlGetProp (node, (const xmlChar *) "tooltip");

        if (value) {
                if (atoi(value) == 1)
                        data->tooltip = TRUE;
                else
                        data->tooltip = FALSE;
                g_free(value);
        }

        if (data->xmldata_options)
                g_array_free(data->xmldata_options, TRUE);

        data->xmldata_options = g_array_new(FALSE, FALSE, sizeof(xmloptions));
        
        for (node = node->children; node; node = node->next) {
                if (node->type != XML_ELEMENT_NODE)
                        continue;

                if (NODE_IS_TYPE(node, "label")) {
                        value = DATA(node);
                        if (value) {
                                gint val = atoi(value); 
                                g_array_append_val(data->xmldata_options, val);
                                g_free(value);
                        }
                }
        }
                        


        /* Update the weather but remove the timeout first */
        if (data->timeout_id)
                gtk_timeout_remove(data->timeout_id);

        update(data, FALSE);

        data->timeout_id = gtk_timeout_add(UPDATETIME, (GtkFunction)cb_update, (gpointer) data);
}

void xfceweather_write_config (Control *control,
                xmlNodePtr parent)
{
        xmlNodePtr root, node;
        gchar *value;
        int i = 0;

        xfceweather_data *data = (xfceweather_data *)control->data;

        root = xmlNewTextChild (parent, NULL, XFCEWEATHER_ROOT, NULL);

        value = g_strdup_printf("%d", data->unit);

        xmlSetProp (root, "celsius", value);

        g_free(value);

        value = g_strdup_printf("%d", data->tooltip);

        xmlSetProp (root, "tooltip", value);

        g_free(value);


        if (data->loc_code)
                xmlSetProp (root, "loc_code", data->loc_code);

        for (i = 0; i < data->xmldata_options->len; i++) {
                xmloptions option = g_array_index (data->xmldata_options, xmloptions, i);
                value = g_strdup_printf("%d", option);

                node = xmlNewTextChild(root, NULL, "label", value);
                g_free(value);
        }
                
         

        //value = g_strdup_printf("%d", data->xmloption_1);
        //xmlSetProp (root, "xmloption_1", value);
        //g_free(value);

        //value = g_strdup_printf("%d", data->xmloption_2);
        //xmlSetProp (root, "xmloption_2", value);
        //g_free(value);

        //value = g_strdup_printf("%d", data->xmloption_3);
        //xmlSetProp (root, "xmloption_3", value);
        //g_free(value);
}

GtkWidget *windowdata_addlabel(gchar *text, gchar *value, GtkSizeGroup *sg, gboolean usepango)
{
        GtkWidget *label, *labelvalue, *hbox = gtk_hbox_new(FALSE, BORDER);
               
        label = gtk_label_new(text);

        if (usepango) {
                labelvalue = gtk_label_new(NULL);
                gtk_label_set_markup(GTK_LABEL(labelvalue), value);
        }
        else
                labelvalue = gtk_label_new(value);
      
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), labelvalue, FALSE, FALSE, 0);

        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_size_group_add_widget(sg, label);

        return hbox;
}

void *windowdata_addforecast_part (GtkTable *table, xmldata_f_p *xmldata_f_p, gint col, gint *row, 
                unit unit)
{
        GtkWidget *image, *label;
        gchar *value;
        gint endcol = col + 1, thisrow = *row, endrow = *row + 1;
        
        value = g_strdup_printf("xfceweather_%s", xmldata_f_p->icon);
        image = gtk_image_new_from_stock(value, GTK_ICON_SIZE_DND);
        g_free(value);
        gtk_table_attach_defaults(table, image, col, endcol, thisrow++, endrow++);

        label = gtk_label_new(xmldata_f_p->t);
        gtk_table_attach_defaults(table, label, col, endcol, thisrow++, endrow++);
        
        label = gtk_label_new(NULL);
        value = g_strdup_printf("%s %s", xmldata_f_p->wind_s, get_unit(unit, WINDSPEED));
        gtk_label_set_markup(GTK_LABEL(label), value);
        g_free(value);
        gtk_table_attach_defaults(table, label, col, endcol, thisrow++, endrow++);

        label = gtk_label_new(xmldata_f_p->wind_t);
        gtk_table_attach_defaults(table, label, col, endcol, thisrow++, endrow++);
        

        label = gtk_label_new(NULL);
        value = g_strdup_printf("%s %s", xmldata_f_p->ppcp, get_unit(unit, HUMIDITY));
        gtk_label_set_markup(GTK_LABEL(label), value);
        g_free(value);
        gtk_table_attach_defaults(table, label, col, endcol, thisrow++, endrow++);

        label = gtk_label_new(NULL);
        value = g_strdup_printf("%s %s", xmldata_f_p->hmid, get_unit(unit, HUMIDITY));
        gtk_label_set_markup(GTK_LABEL(label), value);
        g_free(value);
        gtk_table_attach_defaults(table, label, col, endcol, thisrow++, endrow++);
        
        *row = thisrow;
}

void *windowdata_addforecast (GtkTable *table, xmldata_f *xmldata_f, gint col, unit unit)
{
        GtkWidget *vbox, *image, *label;
        gchar *value;
        gint endcol = col + 1;
        gint row = 0; gint endrow = 1;

        vbox = gtk_vbox_new(FALSE, BORDER);

        label = gtk_label_new(NULL);
        value = g_strdup_printf("<b>%s-%s</b>", xmldata_f->day, xmldata_f->date);
        gtk_label_set_markup(GTK_LABEL(label), value);
        g_free(value);
        gtk_table_attach_defaults(table, label, col, endcol, row++, endrow++);
        
        gtk_table_attach_defaults(table, gtk_hseparator_new(), col, endcol, row++, endrow++);

        /* hi/low */
        label = gtk_label_new(NULL);
        value = g_strdup_printf("%s/%s %s", xmldata_f->hi, xmldata_f->low, get_unit(unit, TEMPERATURE));
        gtk_label_set_markup(GTK_LABEL(label), value);
        g_free(value);
        gtk_table_attach_defaults(table, label, col, endcol, row++, endrow++);

        gtk_table_attach_defaults(table, gtk_hseparator_new(), col, endcol, row++, endrow++);
        
        windowdata_addforecast_part(table, xmldata_f->part_d, col, &row, unit);
        row++;
        windowdata_addforecast_part(table, xmldata_f->part_n, col, &row, unit);

        return vbox;
}
  
GtkWidget *create_windowdata (xmldata *xmldata, GPtrArray *arr_xmldata_f, unit unit)
{
        GtkWidget *window, *vbox, *mainhbox, *hbox, *label, *frame, *vboxitems, *valuelabel, *table, *notebook;
        GtkSizeGroup *sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        gchar *value;
        GdkPixbuf *icon;
        gint i;

        window = gtk_dialog_new_with_buttons("Weather update", NULL,
                        0, 
                        GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

        notebook = gtk_notebook_new();
        
        /* Create image to obtain the pixbuf*/
        value = g_strdup_printf("xfceweather_%d", atoi(xmldata->icon));
        icon = gtk_widget_render_icon(GTK_WIDGET(window), value, GTK_ICON_SIZE_DIALOG,
                        "none");
        g_free(value);
        
        mainhbox = gtk_hbox_new(FALSE, BORDER);
        
                
        vbox = gtk_vbox_new(FALSE, BORDER);
        /* Report information box */
        frame = xfce_framebox_new(_("Report"), TRUE);
        vboxitems = gtk_vbox_new(FALSE, BORDER);

        hbox = windowdata_addlabel(_("Location:"), xmldata->obst, sg, FALSE);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        hbox = windowdata_addlabel(_("Last update:"), xmldata->lsup, sg, FALSE);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        xfce_framebox_add(XFCE_FRAMEBOX(frame), vboxitems);
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

        
        /* Temperature related box */
        frame = xfce_framebox_new(_("Temperature"), TRUE);
        vboxitems = gtk_vbox_new(FALSE, BORDER);
        
        value = g_strdup_printf("%s %s", xmldata->tmp, get_unit(unit, TEMPERATURE));
        hbox = windowdata_addlabel(_("Temperature:"), value, sg, TRUE);
        g_free(value);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        value = g_strdup_printf("%s %s", xmldata->flik, get_unit(unit, TEMPERATURE));
        hbox = windowdata_addlabel(_("Feels like:"), value, sg, TRUE);
        g_free(value);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        hbox = windowdata_addlabel(_("Description:"), xmldata->t, sg, FALSE);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        value = g_strdup_printf("%s %s", xmldata->dewp, get_unit(unit, TEMPERATURE));
        hbox = windowdata_addlabel(_("Dew point:"), value, sg, TRUE);
        g_free(value);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        xfce_framebox_add(XFCE_FRAMEBOX(frame), vboxitems);
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

        /* UV */  
        frame = xfce_framebox_new(_("UV"), TRUE);
        vboxitems = gtk_vbox_new(FALSE, BORDER);

        hbox = windowdata_addlabel(_("UV Index"), xmldata->uv_i, sg, FALSE);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        hbox = windowdata_addlabel(_("UV Risk"), xmldata->uv_t, sg, FALSE);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);
        
        xfce_framebox_add(XFCE_FRAMEBOX(frame), vboxitems);
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
        

        /* Column 1 */
        gtk_box_pack_start(GTK_BOX(mainhbox), vbox, FALSE, FALSE, 0);
     
        vbox = gtk_vbox_new(FALSE, BORDER);

        /* Pressure related */
        frame = xfce_framebox_new(_("Pressure"), TRUE);
        vboxitems = gtk_vbox_new(FALSE, BORDER);

        value = g_strdup_printf("%s %s", xmldata->bar_r, get_unit(unit, PRESSURE));
        hbox = windowdata_addlabel(_("Pressure:"), value, sg, FALSE);
        g_free(value);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        hbox = windowdata_addlabel(_("State:"), xmldata->bar_d, sg, FALSE);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        xfce_framebox_add(XFCE_FRAMEBOX(frame), vboxitems);
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);


        /* Wind related */
        frame = xfce_framebox_new(_("Wind"), TRUE);
        vboxitems = gtk_vbox_new(FALSE, BORDER);

        value = g_strdup_printf("%s %s", xmldata->wind_s, get_unit(unit, WINDSPEED));
        hbox = windowdata_addlabel(_("Speed:"), value, sg, FALSE);
        g_free(value);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        value = g_strdup_printf("%s %s", xmldata->wind_gust, get_unit(unit, WINDGUST));
        hbox = windowdata_addlabel(_("Gust:"), value, sg, FALSE);
        g_free(value);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        hbox = windowdata_addlabel(_("Direction:"), xmldata->wind_t, sg, FALSE);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);
 
        xfce_framebox_add(XFCE_FRAMEBOX(frame), vboxitems);
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);       

        /* other */
        frame = xfce_framebox_new(_("Other"), TRUE);
        vboxitems = gtk_vbox_new(FALSE, BORDER);
        
        value = g_strdup_printf("%s %s", xmldata->hmid, get_unit(unit, HUMIDITY));
        hbox = windowdata_addlabel(_("Humidity:"), value, sg, FALSE);
        g_free(value);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);

        hbox = windowdata_addlabel(_("Visibility:"), xmldata->vis, sg, FALSE);
        gtk_box_pack_start(GTK_BOX(vboxitems), hbox, FALSE, FALSE, 0);
        
        xfce_framebox_add(XFCE_FRAMEBOX(frame), vboxitems);
        gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
        
        gtk_box_pack_start(GTK_BOX(mainhbox), vbox, FALSE, FALSE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(mainhbox), BORDER);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), BORDER); 

        vbox = gtk_vbox_new(FALSE, BORDER);

        label = create_header(icon, _("Weather update"));
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);
        gtk_container_set_border_width(GTK_CONTAINER(notebook), BORDER);
        
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), mainhbox, gtk_label_new("Summary"));

        table = gtk_table_new(DAYS_FORECAST + 1, 14, FALSE);
        gtk_table_set_col_spacings(GTK_TABLE(table), BORDER);
        gtk_container_set_border_width(GTK_CONTAINER(table), BORDER);
        

        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), "<b>Date</b>");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);



        label = gtk_label_new("Hi/low temperature:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 2, 3);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), "<b>Day</b>");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 4, 5);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new("Wind speed:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 6, 7);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new("Wind direction:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 7, 8);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new("Precipitation:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 8, 9);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new("Humidity:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 9, 10);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);  
        
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), "<b>Night</b>");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 11, 12);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new("Wind speed:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 13, 14);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new("Wind direction:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 14, 15);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new("Precipitation:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 15, 16);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        label = gtk_label_new("Humidity:");
        gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 16, 17);

        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        
        for (i = 0; i < arr_xmldata_f->len; i++) {
                xmldata_f *data = (xmldata_f*) g_ptr_array_index(arr_xmldata_f, i);

                windowdata_addforecast(GTK_TABLE(table), data, i + 1, unit);
        }

        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), table, gtk_label_new("Forecast"));
        
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(window)->vbox), vbox, FALSE, FALSE, 0);

        return window;
}

gboolean free_windowdata (GtkDialog *dialog, gint arg1, gpointer user_data)
{
        xfceweather_data *data = (xfceweather_data *)user_data;
        
        gtk_widget_destroy(GTK_WIDGET(dialog));

        data->windowdata_pointer = NULL;

        return FALSE;
}

gboolean cb_click (GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
        xfceweather_data *data = (xfceweather_data *)user_data;
        xmldata *xmldata = data->xmldata;

        if (event->button == 1) {

                if (data->windowdata_pointer)
                        gtk_window_present(GTK_WINDOW(data->windowdata_pointer));
                else {
                        if (data->xmldata->tmp == NULL)
                                return FALSE;
                        data->windowdata_pointer = create_windowdata(data->xmldata, data->xmldata_f, data->unit);
                        gtk_widget_show_all(data->windowdata_pointer);

                        g_signal_connect(data->windowdata_pointer, "response",
                                        G_CALLBACK (free_windowdata), data);
                }
        }
        else if (event->button == 2) {
                if (data->timeout_id)
                        gtk_timeout_remove(data->timeout_id);

                update(data, TRUE);

                data->timeout_id = gtk_timeout_add(UPDATETIME, (GtkFunction)cb_update, (gpointer) data);
        }



        return FALSE;
}

void xfceweather_set_size(Control *control, gint size)
{
        xfceweather_data *data = (xfceweather_data *)control->data;

        data->size = size;

        set_icon(data);
        set_scrollwin(data);
}
/* XXX Not needed
void xfceweather_set_orientation (Control *control, int orientation)
{
        xfceweather_data *data = (xfceweather_data *)control->data;

        //g_object_ref((gpointer *) data->box_labels);
        //g_object_ref((gpointer *) data->image_condition);
 
        gtk_container_remove(GTK_CONTAINER(data->box_widgets), data->box_labels);
        gtk_container_remove(GTK_CONTAINER(data->box_widgets), data->image_condition);

        gtk_widget_destroy(data->box_widgets);
        
        if (orientation == 0) {
                data->box_widgets = gtk_hbox_new(FALSE, 0);

                gtk_box_pack_start(GTK_BOX(data->box_widgets), data->box_labels, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(data->box_widgets), data->image_condition, FALSE, FALSE, 0);
        }
        else if (orientation == 1) {
                data->box_widgets = gtk_vbox_new(FALSE, 0);

                gtk_box_pack_start(GTK_BOX(data->box_widgets), data->image_condition, FALSE, FALSE, 0);
                gtk_box_pack_start(GTK_BOX(data->box_widgets), data->box_labels, FALSE, FALSE, 0);
        }

        g_object_unref((gpointer *) data->box_labels);
        g_object_unref((gpointer *) data->image_condition);

        gtk_container_add(GTK_CONTAINER(data->box_tooltip), data->box_widgets);

        gtk_widget_show_all(data->box_widgets);
}
*/
G_MODULE_EXPORT void
xfce_control_class_init (ControlClass * cc)
{
        cc->name = "weather";
        cc->caption = _("Weather update");

        cc->create_control = (CreateControlFunc) xfceweather_create_control;
        cc->attach_callback = xfceweather_attach_callback;
        cc->read_config = xfceweather_read_config;
        cc->write_config = xfceweather_write_config;

        cc->create_options = xfceweather_create_options;
        cc->free = xfceweather_free;
        cc->set_size = xfceweather_set_size;
        /*cc->set_orientation = xfceweather_set_orientation;*/
}

XFCE_PLUGIN_CHECK_INIT
