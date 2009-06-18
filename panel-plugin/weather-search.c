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

#include <libxfcegui4/libxfcegui4.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#include "weather-search.h"
#include "weather-http.h"

#define BORDER 8



static void
append_result (GtkListStore *mdl,
               gchar        *id,
               gchar        *city)
{
  GtkTreeIter iter;

  gtk_list_store_append (mdl, &iter);
  gtk_list_store_set (mdl, &iter, 0, city, 1, id, -1);
}



static gchar *
sanitize_str (const gchar *str)
{
  GString *retstr = g_string_sized_new (strlen (str));
  gchar   *realstr, c = '\0';

  while ((c = *str++))
    {
      if (g_ascii_isspace (c))
        g_string_append (retstr, "%20");
      else
        g_string_append_c (retstr, c);
    }

  realstr = retstr->str;

  g_string_free (retstr, FALSE);

  return realstr;
}



static void
cb_searchdone (gboolean  succeed,
               gchar    *received,
               gpointer  user_data)
{
  search_dialog *dialog = (search_dialog *) user_data;
  xmlDoc        *doc;
  xmlNode       *cur_node;
  gchar         *id, *city;
  gint           found = 0;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;

  gtk_widget_set_sensitive(dialog->find_button, TRUE);

  if (!succeed || received == NULL)
    return;

  if (g_utf8_validate(received, -1, NULL)) {
    /* force parsing as UTF-8, the XML encoding header may lie */
    doc = xmlReadMemory (received, strlen (received), NULL, "UTF-8", 0);
  } else {
    doc = xmlParseMemory (received, strlen(received));
  }
  g_free (received);

  if (!doc)
    return;

  cur_node = xmlDocGetRootElement (doc);

  if (cur_node)
    {
      for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
        {
          if (NODE_IS_TYPE (cur_node, "loc"))
            {
              id = (gchar *) xmlGetProp (cur_node, (const xmlChar *) "id");

              if (!id)
                continue;

              city = DATA (cur_node);

              if (!city)
                {
                  g_free (id);
                  continue;
                }

              append_result (dialog->result_mdl, id, city);
	      found++;
              g_free (id);
              g_free (city);
            }
        }
    }

  xmlFreeDoc (doc);

  if (found > 0) {
    if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL (dialog->result_mdl), &iter)) {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->result_list));
      gtk_tree_selection_select_iter(selection, &iter);
      gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog->dialog), GTK_RESPONSE_ACCEPT, TRUE);
    }
  }
  
gtk_tree_view_column_set_title(dialog->column, _("Results"));
  return;
}



static void
search_cb (GtkWidget *widget,
           gpointer   user_data)
{
  search_dialog *dialog = (search_dialog *) user_data;
  gchar         *sane_str, *url;
  const gchar   *str;
  str = gtk_entry_get_text (GTK_ENTRY (dialog->search_entry));

  if (strlen (str) == 0)
    return;

  if (dialog->last_search && !strcmp(str, dialog->last_search)) {
    GtkTreeSelection *selection;
    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->result_list));
    if (gtk_tree_selection_count_selected_rows(selection) == 1) {
      gtk_dialog_response(GTK_DIALOG(dialog->dialog), GTK_RESPONSE_ACCEPT);
      return;
    }
  }
  
  g_free(dialog->last_search);
  dialog->last_search = g_strdup(str);

  gtk_list_store_clear (GTK_LIST_STORE (dialog->result_mdl));

  if ((sane_str = sanitize_str (str)) == NULL)
    return;

  gtk_widget_set_sensitive(dialog->find_button, FALSE);
  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog->dialog), GTK_RESPONSE_ACCEPT, FALSE);

  url = g_strdup_printf ("/search/search?where=%s", sane_str);
  g_free (sane_str);

  gtk_tree_view_column_set_title(dialog->column, _("Searching..."));
  weather_http_receive_data ("xoap.weather.com", url,
                             dialog->proxy_host, dialog->proxy_port,
                             cb_searchdone, dialog);

  g_free (url);
}



static void
pass_search_results (GtkTreeView       *tree_view,
                     GtkTreePath       *path,
                     GtkTreeViewColumn *column,
                     GtkDialog         *dialog)
{
  gtk_dialog_response (dialog, GTK_RESPONSE_ACCEPT);
}



search_dialog *
create_search_dialog (GtkWindow *parent,
                      gchar     *proxy_host,
                      gint       proxy_port)
{
  GtkWidget         *vbox, *hbox, *scroll, *frame;
  GtkCellRenderer   *renderer = gtk_cell_renderer_text_new ();
  search_dialog     *dialog;
  GtkWidget         *dialog_vbox, *dialog_action;

  dialog = panel_slice_new0 (search_dialog);

  dialog->proxy_host = proxy_host;
  dialog->proxy_port = proxy_port;

  if (!dialog)
    return NULL;

  dialog->dialog =
    xfce_titled_dialog_new_with_buttons (_("Search weather location code"), parent,
						GTK_DIALOG_MODAL |
						GTK_DIALOG_DESTROY_WITH_PARENT,
						GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
						GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, NULL);

  gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog->dialog), GTK_RESPONSE_ACCEPT, FALSE);

  gtk_window_set_icon_name (GTK_WINDOW (dialog->dialog), GTK_STOCK_FIND);

#if GTK_CHECK_VERSION(2,14,0)
  dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog->dialog));
  dialog_action = gtk_dialog_get_action_area(GTK_DIALOG(dialog->dialog));
#else
  dialog_vbox = GTK_DIALOG (dialog->dialog)->vbox;
  dialog_action = GTK_DIALOG (dialog->dialog)->action_area;
#endif

  vbox = gtk_vbox_new (FALSE, BORDER);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_box_pack_start (GTK_BOX (dialog_vbox), vbox,
                      TRUE, TRUE, 0);
  
  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dialog->dialog),
				   _("Enter a city name or zip code"));

  hbox = gtk_hbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  dialog->search_entry = gtk_entry_new ();
  gtk_box_pack_start (GTK_BOX (hbox), dialog->search_entry, TRUE, TRUE, 0);
  
  g_signal_connect (G_OBJECT (dialog->search_entry), "activate",
                    G_CALLBACK (search_cb), dialog);

  dialog->find_button = gtk_button_new_from_stock (GTK_STOCK_FIND);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->find_button, TRUE, TRUE, 0);
  g_signal_connect (G_OBJECT (dialog->find_button), "clicked",
                    G_CALLBACK (search_cb), dialog);

  frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (vbox), frame, TRUE, TRUE, 0);

  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (frame), scroll);

  dialog->result_mdl = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
  dialog->result_list = gtk_tree_view_new_with_model (GTK_TREE_MODEL (dialog->result_mdl));
  dialog->column = gtk_tree_view_column_new_with_attributes (_("Results"), renderer, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->result_list), dialog->column);
  g_signal_connect (G_OBJECT (dialog->result_list), "row-activated",
                    G_CALLBACK (pass_search_results), dialog->dialog);
  gtk_container_add (GTK_CONTAINER (scroll), dialog->result_list);

  gtk_widget_set_size_request (dialog->dialog, 425, 250);

  return dialog;
}



gboolean
run_search_dialog (search_dialog *dialog)
{
  GtkTreeIter       iter;
  GtkTreeSelection *selection;
  GValue            value = { 0, };

  gtk_widget_show_all (dialog->dialog);

  if (gtk_dialog_run (GTK_DIALOG (dialog->dialog)) == GTK_RESPONSE_ACCEPT)
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->result_list));

      if (gtk_tree_selection_get_selected (selection, NULL, &iter))
        {
          gtk_tree_model_get_value (GTK_TREE_MODEL (dialog->result_mdl),
                                    &iter, 1, &value);
          dialog->result = g_strdup (g_value_get_string (&value));

          g_value_unset (&value);

          gtk_tree_model_get_value (GTK_TREE_MODEL (dialog->result_mdl),
                                    &iter, 0, &value);
          dialog->result_name = g_strdup (g_value_get_string (&value));

          g_value_unset (&value);

          return TRUE;
        }
    }

  return FALSE;
}

void
free_search_dialog (search_dialog * dialog)
{
  g_free (dialog->result);
  g_free (dialog->result_name);
  g_free (dialog->last_search);

  gtk_widget_destroy (dialog->dialog);

  panel_slice_free (search_dialog, dialog);
}

typedef struct {
  const gchar *proxy_host;
  gint proxy_port;
  void (*cb)(const gchar *loc_name, const gchar *loc_code, gpointer user_data);
  gpointer user_data;
} 
geolocation_data;

static void
cb_geo_searchdone (gboolean  succeed,
               gchar    *received,
               gpointer  user_data)
{
  geolocation_data *data = (geolocation_data *) user_data;
  xmlDoc        *doc;
  xmlNode       *cur_node;
  gchar         *id, *city;
  gboolean      found = FALSE;
  GtkTreeIter       iter;
  GtkTreeSelection *selection;

  if (!succeed || received == NULL) {
    data->cb(NULL, NULL, data->user_data);
    g_free(data);
    return;
  }

  if (g_utf8_validate(received, -1, NULL)) {
    /* force parsing as UTF-8, the XML encoding header may lie */
    doc = xmlReadMemory (received, strlen (received), NULL, "UTF-8", 0);
  } else {
    doc = xmlParseMemory (received, strlen(received));
  }
  g_free (received);

  if (!doc) {
    data->cb(NULL, NULL, data->user_data);
    g_free(data);
    return;
  }

  cur_node = xmlDocGetRootElement (doc);

  if (cur_node)
    {
      for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
        {
          if (NODE_IS_TYPE (cur_node, "loc"))
            {
              id = (gchar *) xmlGetProp (cur_node, (const xmlChar *) "id");

              if (!id)
                continue;

              city = DATA (cur_node);

              if (!city)
                {
                  g_free (id);
                  continue;
                }

              data->cb(city, id, data->user_data);
              g_free (id);
              g_free (city);
	      break;
            }
        }
    }
  
  g_free(data);
  xmlFreeDoc (doc);
}

static void
cb_geolocation (gboolean  succeed,
                gchar    *received,
                gpointer  user_data)
{
  geolocation_data *data = (geolocation_data *) user_data;
  xmlDoc        *doc;
  xmlNode       *cur_node;
  gchar         *city = NULL, *country = NULL;
  gchar         *country_code = NULL, *region = NULL;
  gboolean      found = FALSE;

  if (!succeed || received == NULL) {
    data->cb(NULL, NULL, data->user_data);
    g_free(data);
    return;
  }
  if (g_utf8_validate(received, -1, NULL)) {
    /* force parsing as UTF-8, the XML encoding header may lie */
    doc = xmlReadMemory (received, strlen (received), NULL, "UTF-8", 0);
  } else {
    doc = xmlParseMemory (received, strlen(received));
  }
  g_free (received);

  if (!doc) {
    data->cb(NULL, NULL, data->user_data);
    g_free(data);
    return;
  }

  cur_node = xmlDocGetRootElement (doc);

  if (cur_node)
    {
      for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
        {
          if (NODE_IS_TYPE (cur_node, "City"))
            {
              city = DATA (cur_node);
            }
          if (NODE_IS_TYPE (cur_node, "CountryName"))
            {
              country = DATA (cur_node);
            }
          if (NODE_IS_TYPE (cur_node, "CountryCode"))
            {
              country_code = DATA (cur_node);
            }
          if (NODE_IS_TYPE (cur_node, "RegionName"))
            {
              region = DATA (cur_node);
            }
        }
    }
  
  if (country_code && region && !strcmp(country_code, "US")) {
    g_free(country);
    country = region;
    region = NULL;
  }
  g_free(country_code);
  g_free(region);
  
  xmlFreeDoc (doc);

  if (city && country) {
     gchar *full_loc = g_strdup_printf("%s, %s", city, country);
     gchar *url, *sane_str;
     g_free(city);
     g_free(country);
     
     if ((sane_str = sanitize_str (full_loc)) == NULL) {
       data->cb(NULL, NULL, data->user_data);
       g_free(data);
       return;
     }
     g_free(full_loc);
     
     url = g_strdup_printf ("/search/search?where=%s", sane_str);
     g_free (sane_str);

     weather_http_receive_data ("xoap.weather.com", url,
                        	data->proxy_host, data->proxy_port,
                        	cb_geo_searchdone, data);
     g_free(url);
  }
}



gboolean weather_search_by_ip(
	const gchar *proxy_host, gint proxy_port,
        void (*gui_cb)(const gchar *loc_name, const gchar *loc_code, gpointer user_data),
	gpointer user_data)
{
  gchar *city = NULL, *country = NULL;
  gchar *search = NULL;
  geolocation_data *data;
  
  if (!gui_cb)
    return;

  data = g_new0(geolocation_data, 1);
  data->cb = gui_cb;
  data->user_data = user_data;
  data->proxy_host = proxy_host;
  data->proxy_port = proxy_port;

  weather_http_receive_data ("ipinfodb.com", "/ip_query.php",
                             proxy_host, proxy_port,
                             cb_geolocation, data);

}
