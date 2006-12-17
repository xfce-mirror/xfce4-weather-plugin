/*  $Id$
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

#ifndef __WEATHER_CONFIG_H__
#define __WEATHER_CONFIG_H__

G_BEGIN_DECLS

typedef struct
{
  gchar *name;
  datas  number;
}
labeloption;

typedef struct
{
  GtkWidget        *dialog;
  GtkWidget        *opt_unit;
  GtkWidget        *txt_loc_code;
  GtkWidget        *txt_proxy_host;
  GtkWidget        *txt_proxy_port;
  GtkWidget        *chk_proxy_use;
  GtkWidget        *chk_proxy_fromenv;

  GtkWidget        *tooltip_yes;
  GtkWidget        *tooltip_no;

  GtkWidget        *opt_xmloption;
  GtkWidget        *lst_xmloption;
  GtkListStore     *mdl_xmloption;

  xfceweather_data *wd;
}
xfceweather_dialog;

xfceweather_dialog *create_config_dialog (xfceweather_data * data,
                                          GtkWidget * vbox);

void
set_callback_config_dialog (xfceweather_dialog * dialog,
                            void (cb) (xfceweather_data *));

void apply_options (xfceweather_dialog * dialog);

G_END_DECLS

#endif
