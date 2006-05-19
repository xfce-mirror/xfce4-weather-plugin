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

#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

typedef void(*CB_TYPE)(gboolean, gpointer);

gboolean
http_get_file  (gchar *url, gchar *hostname, 
                gchar *filename, gchar *proxy_host, gint proxy_port, 
                CB_TYPE callback, gpointer data);
              
gboolean
http_get_buffer (gchar *url, gchar *hostname, 
                 gchar *proxy_host, gint proxy_port, gchar **buffer,
                 CB_TYPE callback, gpointer data);
#endif
