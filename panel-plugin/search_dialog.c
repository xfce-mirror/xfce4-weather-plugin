#define BORDER 6
#include "parsers.h"
#include "search_dialog.h"
#include "debug_print.h"

void append_result(GtkListStore *mdl, gchar *id, gchar *city)
{
         GtkTreeIter iter;

         gtk_list_store_append(mdl, &iter);
         gtk_list_store_set(mdl, &iter, 0, city, 1, id, -1);
}

gchar *sanitize_str(const gchar *str)
{
        GString *retstr = g_string_sized_new(strlen(str));
        gchar *realstr, c = '\0';

       

        while(c = *str++)
        {
               
                
                if (g_ascii_isspace(c))
                        g_string_append(retstr, "%20");
                else if (g_ascii_isalnum(c) == FALSE)
                {
                        g_string_free(retstr, TRUE);
                        return NULL;
                }
                else
                        g_string_append_c(retstr, c);
        }

        realstr = retstr->str;

        g_string_free(retstr, FALSE);

       

        return realstr;
}
                

gboolean search_cb (GtkButton *button, gpointer user_data)
{
        struct search_dialog *dialog = (struct search_dialog *)user_data;
        gchar *sane_str, *url, *page;
        const gchar *str;
        xmlDocPtr doc;
        xmlNodePtr cur_node;

       

        str = gtk_entry_get_text(GTK_ENTRY(dialog->search_entry));

        if (strlen(str) == 0)
                return FALSE;

        gtk_list_store_clear(GTK_LIST_STORE(dialog->result_mdl));

       

        if ((sane_str = sanitize_str(str)) == NULL)
        {
               
                return FALSE;
        }

        url = g_strdup_printf("/search/search?where=%s", sane_str);
        g_free(sane_str);
        
       

        page = http_get_buffer(url, "xoap.weather.com");
        g_free(url);

        if (!page)
        {
               
                return FALSE;
        }

       

        doc = xmlParseMemory(page, strlen(page));
        g_free(page);

        if (!doc)
        {
               
                return FALSE;
        }

        cur_node = xmlDocGetRootElement(doc);

        if (cur_node)
        {
                for (cur_node = cur_node->children; 
                                cur_node; 
                                cur_node = cur_node->next)
                {
                        if (NODE_IS_TYPE(cur_node, "loc"))
                        {
                                gchar *id = xmlGetProp(cur_node, "id");
                                gchar *city;

                                if (!id)
                                        continue;

                                city = DATA(cur_node);

                                if (!city)
                                { 
                                        g_free(id);
                                        continue;
                                }
                                append_result(dialog->result_mdl, id, city);
                                g_free(id);
                                g_free(city);
                        }
                }
        }

        xmlFreeDoc(doc);

        return FALSE;
}

struct search_dialog *create_search_dialog(GtkWindow *parent)
{
        GtkWidget *vbox, *label, *button, *hbox, *scroll, *frame;
        GtkTreeViewColumn *column;
        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        struct search_dialog *dialog;

        dialog = g_new0(struct search_dialog, 1);
       

        if (!dialog)
                return NULL;

        dialog->dialog = gtk_dialog_new_with_buttons ("Search weather location code",
                        parent,
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_OK,
                        GTK_RESPONSE_ACCEPT,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_REJECT,
                        NULL);
        
        vbox = gtk_vbox_new(FALSE, BORDER);

        label = gtk_label_new("Enter a city name or zip code:");
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        
        dialog->search_entry = gtk_entry_new();
        button = gtk_button_new_from_stock(GTK_STOCK_FIND);
        hbox = gtk_hbox_new(FALSE, BORDER);
        gtk_box_pack_start(GTK_BOX(hbox), dialog->search_entry, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
        
        /* list */
        dialog->result_mdl = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
        dialog->result_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(dialog->result_mdl));

       

        column = gtk_tree_view_column_new_with_attributes("Results", renderer, 
                        "text", 0);
        gtk_tree_view_append_column(GTK_TREE_VIEW(dialog->result_list), column);

        scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), 
                        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scroll), dialog->result_list);

        frame = gtk_frame_new(NULL);
        gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
        gtk_container_add(GTK_CONTAINER(frame), scroll);
        
        gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog->dialog)->vbox), vbox, TRUE, TRUE, 0);

        gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);

        g_signal_connect(button, "clicked", G_CALLBACK(search_cb), dialog);

        gtk_widget_set_size_request(dialog->dialog, 350, 250);

        return dialog;
}
       
gboolean run_search_dialog(struct search_dialog *dialog)
{
        gtk_widget_show_all(dialog->dialog);
        if (gtk_dialog_run(GTK_DIALOG(dialog->dialog)) == GTK_RESPONSE_ACCEPT)
        {
                GtkTreeIter iter;
                GtkTreeSelection *selection = 
                        gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->result_list));
        
                if (gtk_tree_selection_get_selected(selection, NULL, &iter))
                {
                        GValue value = {0, };
                        
                        gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->result_mdl),
                                        &iter, 1, &value);
                        dialog->result = g_strdup(g_value_get_string(&value));

                        g_value_unset(&value);
                        return TRUE;
                }
        }

        return FALSE;
}

void free_search_dialog(struct search_dialog *dialog)
{
        g_free(dialog->result);
        gtk_widget_destroy(dialog->dialog);
}
