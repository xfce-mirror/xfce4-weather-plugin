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

#include <string.h>
#include <libxfce4ui/libxfce4ui.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#include "weather-config.h"
#include "weather-search.h"
#include "weather-scrollbox.h"

#define OPTIONS_N 13
#define BORDER    8

static const labeloption labeloptions[OPTIONS_N] = {
  {N_("Temperature (T)"), TEMPERATURE},
  {N_("Atmosphere pressure (P)"), PRESSURE},
  {N_("Wind speed (WS)"), WIND_SPEED},
  {N_("Wind speed - Beaufort scale (WB)"), WIND_BEAUFORT},
  {N_("Wind direction (WD)"), WIND_DIRECTION},
  {N_("Wind direction in degrees (WD)"), WIND_DIRECTION_DEG},
  {N_("Humidity (H)"), HUMIDITY},
  {N_("Low cloudiness (CL)"), CLOUDINESS_LOW},
  {N_("Medium cloudiness (CM)"), CLOUDINESS_MED},
  {N_("High cloudiness (CH)"), CLOUDINESS_HIGH},
  {N_("Overall cloudiness (C)"), CLOUDINESS_OVERALL},
  {N_("Fog (F)"), FOG},
  {N_("Precipitations (R)"), PRECIPITATIONS},
};

typedef void (*cb_function) (xfceweather_data *);
static cb_function cb = NULL;

static void
add_mdl_option (GtkListStore *mdl,
                gint          opt)
{
  GtkTreeIter iter;

  gtk_list_store_append (mdl, &iter);
  gtk_list_store_set (mdl, &iter,
                      0, _(labeloptions[opt].name),
                      1, labeloptions[opt].number, -1);
}



static gboolean
cb_addoption (GtkWidget *widget,
              gpointer   data)
{
  xfceweather_dialog *dialog;
  gint                history;

  dialog = (xfceweather_dialog *) data;
  history = gtk_option_menu_get_history (GTK_OPTION_MENU (dialog->opt_xmloption));

  add_mdl_option (dialog->mdl_xmloption, history);

  return FALSE;
}



static gboolean
cb_deloption (GtkWidget *widget,
              gpointer   data)
{
  xfceweather_dialog *dialog = (xfceweather_dialog *) data;
  GtkTreeIter         iter;
  GtkTreeSelection    *selection;

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->lst_xmloption));

  if (gtk_tree_selection_get_selected (selection, NULL, &iter))
    gtk_list_store_remove (GTK_LIST_STORE (dialog->mdl_xmloption), &iter);

  return FALSE;
}



static gboolean
cb_toggle (GtkWidget *widget,
           gpointer   data)
{
  GtkWidget *target = (GtkWidget *) data;

  gtk_widget_set_sensitive (target,
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                          (widget)));

  return FALSE;
}



static gboolean
cb_not_toggle (GtkWidget *widget,
               gpointer   data)
{
  GtkWidget *target = (GtkWidget *) data;

  gtk_widget_set_sensitive (target,
                            !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON
                                                           (widget)));

  return FALSE;
}



static GtkWidget *
make_label (void)
{
  guint      i;
  GtkWidget *widget, *menu;

  menu = gtk_menu_new ();
  widget = gtk_option_menu_new ();

  for (i = 0; i < OPTIONS_N; i++)
    {
      labeloption opt = labeloptions[i];

      gtk_menu_shell_append (GTK_MENU_SHELL (menu),
                             gtk_menu_item_new_with_label (_(opt.name)));
    }

  gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);

  return widget;
}



void
apply_options (xfceweather_dialog *dialog)
{
  gint         history = 0;
  gboolean     hasiter = FALSE;
  GtkTreeIter  iter;
  gchar       *text, *pos;
  gint         option;
  GValue       value = { 0, };
  GtkWidget   *widget;

  xfceweather_data *data = (xfceweather_data *) dialog->wd;

  history = gtk_option_menu_get_history (GTK_OPTION_MENU (dialog->opt_unit_system));

  if (history == 0)
    data->unit_system = IMPERIAL;
  else
    data->unit_system = METRIC;

  if (data->lat)
    g_free (data->lat);

  if (data->lon)
    g_free (data->lon);

  if (data->location_name)
    g_free (data->location_name);

  if (data->location_name_short)
    g_free (data->location_name_short);

  data->lat =
    g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->txt_lat)));

  data->lon =
    g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->txt_lon)));

  data->location_name =
    g_strdup (gtk_label_get_text (GTK_LABEL (dialog->txt_loc_name)));

  pos = g_utf8_strchr(data->location_name, -1, ',');
  if (pos != NULL)
    data->location_name_short =
      g_utf8_substring (data->location_name, 0,
        g_utf8_pointer_to_offset (data->location_name, pos));
  else
    data->location_name_short = g_strdup (data->location_name);

  /* call labels_clear() here */
  if (data->labels && data->labels->len > 0)
    g_array_free (data->labels, TRUE);

  data->labels = g_array_new (FALSE, TRUE, sizeof (datas));

  for (hasiter =
       gtk_tree_model_get_iter_first (GTK_TREE_MODEL (dialog->mdl_xmloption),
                                      &iter); hasiter == TRUE;
       hasiter =
       gtk_tree_model_iter_next (GTK_TREE_MODEL (dialog->mdl_xmloption),
                                 &iter))
    {
      gtk_tree_model_get_value (GTK_TREE_MODEL (dialog->mdl_xmloption), &iter,
                                1, &value);
      option = g_value_get_int (&value);
      g_array_append_val (data->labels, option);
      g_value_unset (&value);
    }

  if (data->proxy_host)
    {
      g_free (data->proxy_host);
      data->proxy_host = NULL;
    }

  data->animation_transitions = gtk_toggle_button_get_active(
  	GTK_TOGGLE_BUTTON(dialog->chk_animate_transition));

  gtk_scrollbox_set_animate(GTK_SCROLLBOX(data->scrollbox), data->animation_transitions);

  if (!gtk_toggle_button_get_active
      (GTK_TOGGLE_BUTTON (dialog->chk_proxy_use)))
    data->proxy_fromenv = FALSE;
  else
    if (gtk_toggle_button_get_active
        (GTK_TOGGLE_BUTTON (dialog->chk_proxy_fromenv)))
    {
      data->proxy_fromenv = TRUE;
      check_envproxy (&data->proxy_host, &data->proxy_port);
    }
  else                                /* use provided proxy settings */
    {
      data->proxy_fromenv = FALSE;
      text =
        g_strdup (gtk_entry_get_text (GTK_ENTRY (dialog->txt_proxy_host)));

      if (strlen (text) == 0)
        {
          widget = gtk_message_dialog_new (NULL, GTK_DIALOG_NO_SEPARATOR,
                                           GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
                                           _("Please enter proxy settings"));
          gtk_dialog_run (GTK_DIALOG (widget));
          gtk_widget_destroy (widget);

          gtk_widget_grab_focus (dialog->txt_proxy_host);
          g_free (text);

          return;
        }

      data->proxy_host = g_strdup (text);
      data->proxy_port =
        gtk_spin_button_get_value (GTK_SPIN_BUTTON (dialog->txt_proxy_port));

      if (data->saved_proxy_host)
        g_free (data->saved_proxy_host);

      data->saved_proxy_host = g_strdup (text);
      data->saved_proxy_port = data->proxy_port;

      g_free (text);
    }

  if (cb)
    cb (data);
}



static int
option_i (datas opt)
{
  guint i;

  for (i = 0; i < OPTIONS_N; i++)
    {
      if (labeloptions[i].number == opt)
        return i;
    }

  return -1;
}

static void auto_locate_cb(const gchar *loc_name, const gchar *lat, const gchar *lon, gpointer user_data)
{
  xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;

  if (lat && lon && loc_name) {
    gtk_entry_set_text (GTK_ENTRY (dialog->txt_lat), lat);
    gtk_entry_set_text (GTK_ENTRY (dialog->txt_lon), lon);
    gtk_label_set_text (GTK_LABEL (dialog->txt_loc_name), loc_name);
    gtk_widget_set_sensitive(dialog->txt_loc_name, TRUE);
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text(dialog->txt_loc_name,loc_name);
#endif
  } else {
    gtk_entry_set_text (GTK_ENTRY (dialog->txt_lat), "");
    gtk_entry_set_text (GTK_ENTRY (dialog->txt_lon), "");
    gtk_label_set_text (GTK_LABEL (dialog->txt_loc_name), _("Unset"));
    gtk_widget_set_sensitive(dialog->txt_loc_name, TRUE);
  }
}

static void start_auto_locate(xfceweather_dialog *dialog)
{
  gtk_widget_set_sensitive(dialog->txt_loc_name, FALSE);
  gtk_label_set_text (GTK_LABEL (dialog->txt_loc_name), _("Detecting..."));
  weather_search_by_ip(dialog->wd->proxy_host, dialog->wd->proxy_port,
  	auto_locate_cb, dialog);
}

static gboolean
cb_findlocation (GtkButton *button,
                 gpointer   user_data)
{
  xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
  search_dialog *sdialog;

  sdialog = create_search_dialog (NULL,
                                  dialog->wd->proxy_host,
                                  dialog->wd->proxy_port);

  if (run_search_dialog (sdialog)) {
    gtk_entry_set_text (GTK_ENTRY (dialog->txt_lat), sdialog->result_lat);
    gtk_entry_set_text (GTK_ENTRY (dialog->txt_lon), sdialog->result_lon);
    gtk_label_set_text (GTK_LABEL (dialog->txt_loc_name), sdialog->result_name);
    gtk_widget_set_sensitive(dialog->txt_loc_name, TRUE);
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text(dialog->txt_loc_name,sdialog->result_name);
#endif
  }
  free_search_dialog (sdialog);

  return FALSE;
}



xfceweather_dialog *
create_config_dialog (xfceweather_data *data,
                      GtkWidget        *vbox)
{
  xfceweather_dialog *dialog;
  GtkWidget          *vbox2, *vbox3, *hbox, *hbox2, *label,
                     *menu, *button_add, *button_del, *image, *button, *scroll;
  GtkSizeGroup       *sg, *sg_buttons;
  GtkTreeViewColumn  *column;
  GtkCellRenderer    *renderer;
  datas               opt;
  guint               i;
  gint                n;

  sg = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
  sg_buttons = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

  dialog = g_slice_new0 (xfceweather_dialog);

  dialog->wd = (xfceweather_data *) data;
  dialog->dialog = gtk_widget_get_toplevel (vbox);

  label = gtk_label_new (_("System of Measurement:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  menu = gtk_menu_new ();
  dialog->opt_unit_system = gtk_option_menu_new ();

  gtk_menu_shell_append ((GtkMenuShell *) (GTK_MENU (menu)),
                         (gtk_menu_item_new_with_label (_("Imperial"))));
  gtk_menu_shell_append ((GtkMenuShell *) (GTK_MENU (menu)),
                         (gtk_menu_item_new_with_label (_("Metric"))));
  gtk_option_menu_set_menu (GTK_OPTION_MENU (dialog->opt_unit_system), menu);

  if (dialog->wd->unit_system == IMPERIAL)
    gtk_option_menu_set_history (GTK_OPTION_MENU (dialog->opt_unit_system), 0);
  else
    gtk_option_menu_set_history (GTK_OPTION_MENU (dialog->opt_unit_system), 1);
  gtk_size_group_add_widget (sg, label);

  hbox = gtk_hbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->opt_unit_system, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);


  label = gtk_label_new (_("Location:"));
  dialog->txt_lat = gtk_entry_new ();
  dialog->txt_lon = gtk_entry_new ();
  dialog->txt_loc_name = gtk_label_new ("");

  gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
  gtk_misc_set_alignment (GTK_MISC (dialog->txt_loc_name), 0, 0.5);

#if GTK_CHECK_VERSION(2,12,0)
  gtk_label_set_ellipsize (GTK_LABEL(dialog->txt_loc_name), PANGO_ELLIPSIZE_END);
#endif
  if (dialog->wd->lat != NULL)
    gtk_entry_set_text (GTK_ENTRY (dialog->txt_lat),
                        dialog->wd->lat);
  if (dialog->wd->lon != NULL)
    gtk_entry_set_text (GTK_ENTRY (dialog->txt_lon),
                        dialog->wd->lon);

  if (dialog->wd->location_name != NULL)
    gtk_label_set_text (GTK_LABEL (dialog->txt_loc_name),
                        dialog->wd->location_name);

#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(dialog->txt_loc_name,
  	gtk_label_get_text(GTK_LABEL(dialog->txt_loc_name)));
#endif

  if (dialog->wd->lat == NULL || dialog->wd->lon == NULL) {
    start_auto_locate(dialog);
  }
  gtk_size_group_add_widget (sg, label);

  button = gtk_button_new_with_label(_("Change..."));
  image = gtk_image_new_from_stock (GTK_STOCK_FIND, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (cb_findlocation), dialog);

  hbox = gtk_hbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->txt_loc_name, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  /* proxy */
  label = gtk_label_new (_("Proxy server:"));
  dialog->txt_proxy_host = gtk_entry_new ();
  dialog->chk_proxy_use =
    gtk_check_button_new_with_label (_("Use proxy server"));
  dialog->chk_proxy_fromenv =
    gtk_check_button_new_with_label (_("Auto-detect from environment"));
  dialog->txt_proxy_port = gtk_spin_button_new_with_range (0, 65536, 1);

  gtk_misc_set_alignment (GTK_MISC (label), 0, 0);

  gtk_size_group_add_widget (sg, label);

  vbox3 = gtk_vbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (vbox3), dialog->chk_proxy_use, FALSE, FALSE,
                      0);
  gtk_box_pack_start (GTK_BOX (vbox3), dialog->chk_proxy_fromenv, FALSE,
                      FALSE, 0);

  hbox = gtk_hbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->txt_proxy_host, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->txt_proxy_port, FALSE, FALSE,
                      0);
  gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);


  hbox2 = gtk_hbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (hbox2), label, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox2), vbox3, TRUE, TRUE, 0);

  gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (dialog->chk_proxy_use), "toggled",
                    G_CALLBACK (cb_toggle), dialog->txt_proxy_host);
  g_signal_connect (G_OBJECT (dialog->chk_proxy_use), "toggled",
                    G_CALLBACK (cb_toggle), dialog->txt_proxy_port);
  g_signal_connect (G_OBJECT (dialog->chk_proxy_use), "toggled",
                    G_CALLBACK (cb_toggle), dialog->chk_proxy_fromenv);
  g_signal_connect (G_OBJECT (dialog->chk_proxy_fromenv), "toggled",
                    G_CALLBACK (cb_not_toggle), dialog->txt_proxy_host);
  g_signal_connect (G_OBJECT (dialog->chk_proxy_fromenv), "toggled",
                    G_CALLBACK (cb_not_toggle), dialog->txt_proxy_port);

  if (dialog->wd->saved_proxy_host != NULL)
    {
      gtk_entry_set_text (GTK_ENTRY (dialog->txt_proxy_host),
                          dialog->wd->saved_proxy_host);

      gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->txt_proxy_port),
                                 dialog->wd->saved_proxy_port);
    }

  if (dialog->wd->proxy_host || dialog->wd->proxy_fromenv)
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->chk_proxy_use),
                                    TRUE);

      if (dialog->wd->proxy_fromenv)
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                      (dialog->chk_proxy_fromenv), TRUE);
    }
  else
    {
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->chk_proxy_use),
                                    TRUE);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->chk_proxy_use),
                                    FALSE);
    }


  /* labels */
  dialog->opt_xmloption = make_label ();
  dialog->mdl_xmloption = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
  dialog->lst_xmloption =
    gtk_tree_view_new_with_model (GTK_TREE_MODEL (dialog->mdl_xmloption));

  renderer = gtk_cell_renderer_text_new ();
  column =
    gtk_tree_view_column_new_with_attributes (_("Labels to display"),
                                              renderer, "text", 0, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->lst_xmloption), column);

  button_add = gtk_button_new_from_stock (GTK_STOCK_ADD);
  gtk_size_group_add_widget (sg_buttons, button_add);
  hbox = gtk_hbox_new (FALSE, BORDER);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->opt_xmloption, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), button_add, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

  button_del = gtk_button_new_from_stock (GTK_STOCK_REMOVE);
  gtk_size_group_add_widget (sg_buttons, button_del);
  hbox = gtk_hbox_new (FALSE, BORDER);
  scroll = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (scroll), dialog->lst_xmloption);
  gtk_box_pack_start (GTK_BOX (hbox), scroll, TRUE, TRUE, 0);

  vbox2 = gtk_vbox_new (FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox2), button_del, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_set_size_request (dialog->lst_xmloption, -1, 120);


  if (data->labels->len > 0)
    {
      for (i = 0; i < data->labels->len; i++)
        {
          opt = g_array_index (data->labels, datas, i);

          if ((n = option_i (opt)) != -1)
            add_mdl_option (dialog->mdl_xmloption, n);
        }
    }

  g_object_unref (G_OBJECT (sg));
  g_object_unref (G_OBJECT (sg_buttons));

  g_signal_connect (G_OBJECT (button_add), "clicked",
                    G_CALLBACK (cb_addoption), dialog);
  g_signal_connect (G_OBJECT (button_del), "clicked",
                    G_CALLBACK (cb_deloption), dialog);



  dialog->chk_animate_transition =
    gtk_check_button_new_with_label (_("Animate transitions between labels"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
	 (dialog->chk_animate_transition), dialog->wd->animation_transitions);
  gtk_box_pack_start (GTK_BOX (vbox), dialog->chk_animate_transition, FALSE, FALSE, 0);

  gtk_widget_show_all (vbox);

  return dialog;
}



void
set_callback_config_dialog (xfceweather_dialog *dialog,
                            cb_function         cb_new)
{
  cb = cb_new;
}
