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

#ifndef __WEATHER_H__
#define __WEATHER_H__

#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4util/libxfce4util.h>

G_BEGIN_DECLS

typedef struct
{
  XfcePanelPlugin *plugin;

  GtkTooltips     *tooltips;

  GtkWidget       *scrollbox;
  GtkWidget       *iconimage;
  GtkWidget       *tooltipbox;

  GtkWidget       *summary_window;

  GArray          *labels;

  gint             size;
  gint             updatetimeout;

  gchar           *location_code;

  units            unit;

  xml_weather     *weatherdata;

  gchar           *proxy_host;
  gint             proxy_port;

  gboolean         proxy_fromenv;
  /* used for storing the configured
   * but not active proxy setttings */
  gchar           *saved_proxy_host;
  gint             saved_proxy_port;
}
xfceweather_data;

gboolean check_envproxy (gchar ** proxy_host, gint * proxy_port);

G_END_DECLS

#endif
