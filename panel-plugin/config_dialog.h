#ifndef CONFIG_DIALOG_H
#define CONFIG_DIALOG_H

#include "search_dialog.h"
#include "plugin.h"
#include <gtk/gtk.h>

#include <config.h>

#define OPTIONS_N 11

struct labeloption {
        gchar *name;
        enum datas number;
};

struct xfceweather_dialog {
        GtkWidget *dialog;
        GtkWidget *opt_unit;
        GtkWidget *txt_loc_code;
        GtkWidget *txt_proxy_host;
        GtkWidget *txt_proxy_port;

        GtkWidget *tooltip_yes;
        GtkWidget *tooltip_no;

        GtkWidget *opt_xmloption;
        GtkWidget *lst_xmloption;
        GtkListStore *mdl_xmloption;

        struct xfceweather_data *wd;
};

struct xfceweather_dialog *create_config_dialog(struct xfceweather_data *data,
                GtkContainer *container, 
                GtkWidget *done);

void set_callback_config_dialog(struct xfceweather_dialog *dialog, 
                void(cb)(struct xfceweather_data *));
#endif
