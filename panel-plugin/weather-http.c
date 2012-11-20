/* Copyright (c) 2003-2012 Xfce Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "weather-http.h"

#define WEATHER_MAX_CONN_TIMEOUT   (10)        /* connection timeout in seconds */


void
weather_http_queue_request(const gchar *uri,
                           SoupSessionCallback callback_func,
                           gpointer user_data)
{
    SoupSession *session;
    SoupMessage *msg;
    SoupURI *soup_proxy_uri;
    const gchar *proxy_uri;

    /* create a new soup session */
    session = soup_session_async_new();
    g_object_set(session, SOUP_SESSION_TIMEOUT,
                 WEATHER_MAX_CONN_TIMEOUT, NULL);

    /* Set the proxy URI if any */
    proxy_uri = g_getenv("HTTP_PROXY");
    if (!proxy_uri)
        proxy_uri = g_getenv("http_proxy");
    if (proxy_uri) {
        soup_proxy_uri = soup_uri_new (proxy_uri);
        g_object_set(session, SOUP_SESSION_PROXY_URI, soup_proxy_uri, NULL);
        soup_uri_free(soup_proxy_uri);
    }

    msg = soup_message_new("GET", uri);
    soup_session_queue_message(session, msg, callback_func, user_data);
}
