#include <config.h>

#include "config_dialog.h"
#include "debug_print.h"
#include "parsers.h"
#include "get_data.h"

#include <libxfce4util/libxfce4util.h>

struct labeloption labeloptions[OPTIONS_N] = { 
                {N_("Windchill (F)"), FLIK},
                {N_("Temperature (T)"), TEMP},
                {N_("Atmosphere pressure (P)"), BAR_R},
                {N_("Atmosphere state (P)"), BAR_D},
                {N_("Wind speed (WS)"), WIND_SPEED},
                {N_("Wind gust (WG)"), WIND_GUST},
                {N_("Wind direction (WD)"), WIND_DIRECTION},
                {N_("Humidity (H)"), HMID},
                {N_("Visibility (V)"), VIS},
                {N_("UV Index (UV)"), UV_INDEX},
                {N_("Dewpoint (DP)"), DEWP}
};

typedef void(*cb_function)(struct xfceweather_data *);
static cb_function cb = NULL;

void add_mdl_option(GtkListStore *mdl, int opt)
{
        GtkTreeIter iter;
        
        gtk_list_store_append(mdl, &iter);
        gtk_list_store_set(mdl, &iter,
                        0, _(labeloptions[opt].name),
                        1, labeloptions[opt].number,
                        -1);
}

gboolean cb_addoption (GtkWidget *widget, gpointer data)
{
        struct xfceweather_dialog *dialog = (struct xfceweather_dialog *)data;
        gint history = gtk_option_menu_get_history(GTK_OPTION_MENU(dialog->opt_xmloption)); 

        add_mdl_option(dialog->mdl_xmloption, history);

        return FALSE;
}

gboolean cb_deloption (GtkWidget *widget, gpointer data)
{
        struct xfceweather_dialog *dialog = (struct xfceweather_dialog *)data;
        GtkTreeIter iter;
        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->lst_xmloption));
        
        if (gtk_tree_selection_get_selected(selection, NULL, &iter))
                gtk_list_store_remove(GTK_LIST_STORE(dialog->mdl_xmloption), &iter);

        return FALSE;
}

gboolean cb_toggle(GtkWidget *widget, gpointer data)
{
        GtkWidget *target = (GtkWidget *)data;

        gtk_widget_set_sensitive(target,
                        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));

        return FALSE;
}

gboolean cb_not_toggle(GtkWidget *widget, gpointer data)
{
        GtkWidget *target = (GtkWidget *)data;

        gtk_widget_set_sensitive(target,
                        !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));

        return FALSE;
}                       

static GtkWidget *make_label(void)
{
        int i;
        GtkWidget *widget, *menu;
        
        menu = gtk_menu_new();
        widget = gtk_option_menu_new();

        for (i = 0; i < 11; i++)
        {
                struct labeloption opt = labeloptions[i];

                gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                                gtk_menu_item_new_with_label(_(opt.name)));
        }
        
        gtk_option_menu_set_menu(GTK_OPTION_MENU(widget), menu);

        return widget;
}

void apply_options (struct xfceweather_dialog *dialog)
{
        int history = 0;
        gboolean hasiter = FALSE;
        GtkTreeIter iter;
        gchar *value;

        struct xfceweather_data *data = (struct xfceweather_data *)dialog->wd;
        
        history = gtk_option_menu_get_history(GTK_OPTION_MENU(dialog->opt_unit));

        if (history == 0)
                data->unit = IMPERIAL;
        else 
                data->unit = METRIC;

        if (data->location_code)
                g_free(data->location_code);

        data->location_code = g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog->txt_loc_code))); 

        /* call labels_clear() here */
        if (data->labels && data->labels->len > 0)
                g_array_free(data->labels, TRUE);

        data->labels = g_array_new(FALSE, TRUE, sizeof(enum datas));

        for (hasiter = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(dialog->mdl_xmloption), &iter);
                        hasiter == TRUE; hasiter = gtk_tree_model_iter_next(
                                GTK_TREE_MODEL(dialog->mdl_xmloption), &iter))
        {
                gint option;
                GValue value = {0, };

                gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->mdl_xmloption), &iter, 1, &value);
                option = g_value_get_int(&value);
                g_array_append_val(data->labels, option);
                g_value_unset(&value);
        }
        
        if (data->proxy_host)
        {
                g_free(data->proxy_host);
                data->proxy_host = NULL; 
        }
        
        if (!gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->chk_proxy_use)))
                data->proxy_fromenv = FALSE;
        else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dialog->chk_proxy_fromenv)))
        {
                data->proxy_fromenv = TRUE;
                check_envproxy(&data->proxy_host, &data->proxy_port);
        }
        else /* use provided proxy settings */
        {
                data->proxy_fromenv = FALSE;
                value = (gchar *)gtk_entry_get_text(GTK_ENTRY(dialog->txt_proxy_host));

                if (strlen(value) == 0)
                {
                        xfce_err(_("Please enter proxy settings"));
                        gtk_widget_grab_focus(dialog->txt_proxy_host);
                        return;
                }

                data->proxy_host = g_strdup(value);
                data->proxy_port = gtk_spin_button_get_value(
                                GTK_SPIN_BUTTON(dialog->txt_proxy_port));

                if (data->saved_proxy_host)
                        g_free(data->saved_proxy_host);
                
                data->saved_proxy_host = g_strdup(value);
                data->saved_proxy_port = data->proxy_port;
        }       

        if (cb)
                cb(data);
}

int option_i(enum datas opt)
{
        int i;

        for (i = 0; i < OPTIONS_N; i++)
        {
                if (labeloptions[i].number == opt)
                        return i;
        }

        return -1;
}

gboolean cb_findlocation(GtkButton *button, gpointer user_data)
{
        struct xfceweather_dialog *dialog = (struct xfceweather_dialog *)user_data;
        struct search_dialog *sdialog = create_search_dialog(NULL, 
                        dialog->wd->proxy_host, dialog->wd->proxy_port);

        if (run_search_dialog(sdialog))
                gtk_entry_set_text(GTK_ENTRY(dialog->txt_loc_code), sdialog->result);

        free_search_dialog(sdialog);

        return FALSE;
}
        

struct xfceweather_dialog *create_config_dialog(struct xfceweather_data *data,
                GtkWidget *vbox)
{
        struct xfceweather_dialog *dialog;
        GtkWidget *vbox2, *vbox3, *hbox, *hbox2, *label, 
              *menu, *button_add, 
              *button_del, *image, *button, *scroll;
        GtkSizeGroup *sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        GtkSizeGroup *sg_buttons = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
        GtkTreeViewColumn *column;
        GtkCellRenderer *renderer; 
        
        dialog = g_new0(struct xfceweather_dialog, 1);
 
        dialog->wd = (struct xfceweather_data *)data;
        dialog->dialog = gtk_widget_get_toplevel(vbox);

        label = gtk_label_new(_("Measurement unit:"));
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        menu = gtk_menu_new();
        dialog->opt_unit = gtk_option_menu_new();
       
        gtk_menu_shell_append  ((GtkMenuShell *)(GTK_MENU(menu)),
                                (gtk_menu_item_new_with_label(_("Imperial"))));
        gtk_menu_shell_append  ((GtkMenuShell *)(GTK_MENU(menu)),
                                (gtk_menu_item_new_with_label(_("Metric"))));
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


        label = gtk_label_new(_("Location code:"));
        dialog->txt_loc_code = gtk_entry_new();

        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

        if (dialog->wd->location_code != NULL)
                gtk_entry_set_text(GTK_ENTRY(dialog->txt_loc_code), 
                                dialog->wd->location_code);
        gtk_size_group_add_widget(sg, label);

        button = gtk_button_new();
        image = gtk_image_new_from_stock(GTK_STOCK_FIND, GTK_ICON_SIZE_BUTTON);
        gtk_container_add(GTK_CONTAINER(button), image);
        g_signal_connect(button, "clicked", G_CALLBACK(cb_findlocation), dialog);

        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->txt_loc_code, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

        /* proxy */
        label = gtk_label_new(_("Proxy server:"));
        dialog->txt_proxy_host = gtk_entry_new();
        dialog->chk_proxy_use = gtk_check_button_new_with_label(_("Use proxy server"));
        dialog->chk_proxy_fromenv = 
                gtk_check_button_new_with_label(_("Auto-detect from environment"));
        dialog->txt_proxy_port = gtk_spin_button_new_with_range(0, 65536, 1);

        gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

        gtk_size_group_add_widget(sg, label);

        vbox3 = gtk_vbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(vbox3), dialog->chk_proxy_use, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox3), dialog->chk_proxy_fromenv, FALSE, FALSE, 0);

        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->txt_proxy_host, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->txt_proxy_port, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox3), hbox, FALSE, FALSE, 0);
        

        hbox2 = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox2), vbox3, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

        g_signal_connect(dialog->chk_proxy_use, "toggled", G_CALLBACK(cb_toggle),
                        (gpointer)dialog->txt_proxy_host);
        g_signal_connect(dialog->chk_proxy_use, "toggled", G_CALLBACK(cb_toggle),
                        (gpointer)dialog->txt_proxy_port);
        g_signal_connect(dialog->chk_proxy_use, "toggled", G_CALLBACK(cb_toggle),
                        (gpointer)dialog->chk_proxy_fromenv);

        g_signal_connect(dialog->chk_proxy_fromenv, "toggled", G_CALLBACK(cb_not_toggle),
                        (gpointer)dialog->txt_proxy_host);
        g_signal_connect(dialog->chk_proxy_fromenv, "toggled", G_CALLBACK(cb_not_toggle),
                        (gpointer)dialog->txt_proxy_port);

        if (dialog->wd->saved_proxy_host != NULL)
        {
                gtk_entry_set_text(GTK_ENTRY(dialog->txt_proxy_host), 
                                dialog->wd->saved_proxy_host); 

                gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->txt_proxy_port), 
                                dialog->wd->saved_proxy_port);
        }

        if (dialog->wd->proxy_host || dialog->wd->proxy_fromenv)
        { 
                gtk_toggle_button_set_active(
                                GTK_TOGGLE_BUTTON(dialog->chk_proxy_use), TRUE);
                
                if (dialog->wd->proxy_fromenv)
                        gtk_toggle_button_set_active(
                                        GTK_TOGGLE_BUTTON(dialog->chk_proxy_fromenv), TRUE);
        }
        else
        { 
                gtk_toggle_button_set_active(
                                GTK_TOGGLE_BUTTON(dialog->chk_proxy_use), TRUE);
                gtk_toggle_button_set_active(
                                GTK_TOGGLE_BUTTON(dialog->chk_proxy_use), FALSE);
        }


        /* labels */
         
        dialog->opt_xmloption = make_label(); 
        dialog->mdl_xmloption = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
        dialog->lst_xmloption = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->mdl_xmloption));

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes(_("Labels to display"), renderer,
                        "text", 0, NULL);
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


        if (data->labels->len > 0) 
        {
                enum datas opt;
                gint i, n;
                
                for (i = 0; i < data->labels->len; i++) 
                {
                        opt = g_array_index (data->labels, enum datas, i);

                        if ((n = option_i(opt)) != -1)
                                add_mdl_option(dialog->mdl_xmloption, n);
                }
        }

        g_signal_connect(button_add, "clicked", G_CALLBACK(cb_addoption), dialog);
        g_signal_connect(button_del, "clicked", G_CALLBACK(cb_deloption), dialog);      
        
        gtk_widget_show_all(vbox);

        return dialog;
}

void set_callback_config_dialog(struct xfceweather_dialog *dialog, 
                cb_function cb_new)
{
        cb = cb_new;
}
