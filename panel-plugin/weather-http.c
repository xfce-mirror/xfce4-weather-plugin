/* $Id$ */
/*
 * Copyright (c) 2007 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/xfce-panel-macros.h>

#include "weather-http.h"



#define WEATHER_MAX_CONN_TIMEOUT   (10)        /* connection timeout in seconds */
#define WEATHER_RESCHEDULE_TIMEOUT (30 * 1000) /* reschedule timeout in ms */
#define WEATHER_RESCHEDULE_N_TIMES (5)         /* maximum number or reschedule tries */



/* global */
static GSList *qeued_transfers = NULL;



enum
{
  STATUS_NOT_EXECUTED,
  STATUS_RUNNING,
  STATUS_SUCCEED,
  STATUS_RESCHEDULE,
  STATUS_ERROR,
  STATUS_LEAVE_IMMEDIATELY,
  STATUS_TIMEOUT
};

typedef struct _WeatherConnection WeatherConnection;
struct _WeatherConnection
{
  /* thread id */
  gint         id;

  /* reschedule counter */
  guint        counter;

  /* connection data */
  gchar       *hostname;
  gchar       *url;
  gchar       *proxy_host;
  gint         proxy_port;

  /* receive status */
  gint         status;

  /* received data */
  GString     *received;

  /* connection descriptor */
  gint         fd;

  /* callback data */
  WeatherFunc  cb_func;
  gpointer     cb_user_data;
};



static gboolean
weather_http_receive_data_check (WeatherConnection *connection,
                                 GTimeVal           timeout)
{
  GTimeVal now;

  /* check if we need to leave */
  if (G_UNLIKELY (connection->status == STATUS_LEAVE_IMMEDIATELY))
    return TRUE;

  /* get the current time */
  g_get_current_time (&now);

  /* check if we timed out */
  if (G_UNLIKELY ((gint) timeout.tv_sec + WEATHER_MAX_CONN_TIMEOUT < (gint) now.tv_sec))
    {
      /* set status */
      connection->status = STATUS_TIMEOUT;

      return TRUE;
    }

  return FALSE;
}



static gboolean
weather_http_receive_data_idle (gpointer user_data)
{
  WeatherConnection  *connection = user_data;
  gchar               buffer[1024];
  gint                bytes, n, m;
  gchar              *request;
  struct hostent       *host;
  struct sockaddr_in  sockaddr;
  const gchar        *p, *hostname;
  GTimeVal            timeout;

  /* set the current time */
  g_get_current_time (&timeout);

  /* hostname we're going to use */
  hostname = connection->proxy_host ? connection->proxy_host : connection->hostname;

  /* try to get the hostname */
  host = gethostbyname (hostname);
  if (G_UNLIKELY (host == NULL))
    {
      /* display error */
      g_message (_("Failed to get the hostname \"%s\". Retry in %d seconds."),
                 hostname, WEATHER_RESCHEDULE_TIMEOUT / 1000);

      /* try again later */
      connection->status = STATUS_RESCHEDULE;

      return FALSE;
    }

  if (weather_http_receive_data_check (connection, timeout))
    return FALSE;

  /* open the socket */
  connection->fd = socket (PF_INET, SOCK_STREAM, 0);
  if (G_UNLIKELY (connection->fd < 0))
    {
      /* display error */
      g_error (_("Failed to open the socket (%s)."), g_strerror (errno));

      /* set status */
      connection->status = STATUS_ERROR;

      return FALSE;
    }

  if (weather_http_receive_data_check (connection, timeout))
    return FALSE;

  /* complete the host information */
  sockaddr.sin_family = PF_INET;
  sockaddr.sin_port = htons (connection->proxy_port ? connection->proxy_port : 80);
  sockaddr.sin_addr = *((struct in_addr *)host->h_addr);
  memset(&(sockaddr.sin_zero), '\0', 8);

  /* open a connection with the host */
  m = connect (connection->fd, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr));
  if (G_UNLIKELY (m < 0))
    {
      /* display error */
      g_error (_("Failed to create a connection with the host (%s)."), g_strerror (errno));

      /* set status */
      connection->status = STATUS_ERROR;

      return FALSE;
    }

  if (weather_http_receive_data_check (connection, timeout))
    return FALSE;

  /* create the request */
  if (connection->proxy_host)
    request = g_strdup_printf ("GET http://%s%s HTTP/1.0\r\n\r\n", hostname, connection->url);
  else
    request = g_strdup_printf ("GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", connection->url, hostname);

  /* send the request */
  for (m = 0, n = strlen (request); m < n;)
    {
      /* send some info to the host */
      bytes = send (connection->fd, request + m, n - m, 0);

      if (weather_http_receive_data_check (connection, timeout))
        return FALSE;

      /* check for problems */
      if (G_UNLIKELY (bytes < 0))
        {
          /* just try again on EAGAIN/EINTR */
          if (G_LIKELY (errno != EAGAIN && errno != EINTR))
            {
              /* display error */
              g_error (_("Failed to send the request (%s)."), g_strerror (errno));

              /* cleanup the request */
              g_free (request);

              /* set status */
              connection->status = STATUS_ERROR;

              return FALSE;
            }
        }
      else
        {
          /* advance the offset */
          m += bytes;
        }
    }

  /* cleanup the request */
  g_free (request);

  /* create and empty string */
  connection->received = g_string_sized_new (5000);

  /* download the file content */
  do
    {
      /* download some bytes */
      bytes = recv (connection->fd, buffer, sizeof (buffer) - sizeof (gchar), 0);

      if (weather_http_receive_data_check (connection, timeout))
        return FALSE;

      /* check for problems */
      if (G_UNLIKELY (bytes < 0))
        {
          /* display error */
          g_error (_("Failed to receive data (%s)"), g_strerror (errno));

          /* set status */
          connection->status = STATUS_ERROR;

          return FALSE;
        }

      /* prepend the downloaded data */
      connection->received = g_string_append_len (connection->received, buffer, bytes);
    }
  while (bytes > 0);

  if (weather_http_receive_data_check (connection, timeout))
    return FALSE;

  if (G_LIKELY (connection->received->len > 0))
    {
      /* get the pointer to the content-length */
      p = strstr (connection->received->str, "Content-Length:");

      if (G_LIKELY (p))
        {
          /* advance the pointer */
          p += 15;

          /* calculate the header length */
          n = connection->received->len - strtol (p, NULL, 10);

          if (G_LIKELY (n > 0))
            {
              /* erase the header from the reveiced string */
              g_string_erase (connection->received, 0, n);
            }

          /* everything went fine... */
          connection->status = STATUS_SUCCEED;
        }
      else
        {
          /* display error */
          g_error (_("Unable to detect the content length."));

          /* set status */
          connection->status = STATUS_ERROR;
        }
    }
  else
    {
      g_error (_("No content received."));

      /* set status */
      connection->status = STATUS_ERROR;
    }

  return FALSE;
}



static void
weather_http_receive_data_destroyed (gpointer user_data)
{
  WeatherConnection *connection = user_data;

  /* close the socket */
  if (connection->fd != -1)
    {
      close (connection->fd);
      connection->fd = -1;
    }

  if (connection->status == STATUS_TIMEOUT)
    g_message ("Connection timeout");

  if (connection->status == STATUS_SUCCEED && connection->cb_func)
    {
      /* execute the callback process */
      (*connection->cb_func) (TRUE, g_string_free (connection->received, FALSE),
                              connection->cb_user_data);
    }
  else if (connection->status == STATUS_RESCHEDULE &&
           connection->counter < WEATHER_RESCHEDULE_N_TIMES)
    {
      /* cleanup the received data */
      if (connection->received)
        {
          g_string_free (connection->received, TRUE);
          connection->received = NULL;
        }

      /* increase counter */
      connection->counter++;

      /* reschedule request */
      connection->id = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, WEATHER_RESCHEDULE_TIMEOUT,
                                           weather_http_receive_data_idle, connection,
                                           weather_http_receive_data_destroyed);

      /* leave without freeing data */
      return;
    }
  else
    {
      /* execute empty callback */
      if (connection->cb_func)
        (*connection->cb_func) (FALSE, NULL, connection->cb_user_data);

      /* cleanup */
      if (connection->received)
        g_string_free (connection->received, TRUE);
    }

  /* remove from the list */
  qeued_transfers = g_slist_remove (qeued_transfers, connection);

  /* free other data */
  g_free (connection->hostname);
  g_free (connection->url);
  g_free (connection->proxy_host);

  /* cleanup */
  panel_slice_free (WeatherConnection, connection);
}



void
weather_http_receive_data (const gchar  *hostname,
                           const gchar  *url,
                           const gchar  *proxy_host,
                           gint          proxy_port,
                           WeatherFunc   cb_func,
                           gpointer      user_data)
{
  WeatherConnection *connection;

  /* create slice */
  connection = panel_slice_new0 (WeatherConnection);

  /* set connection properties */
  connection->hostname = g_strdup (hostname);
  connection->url = g_strdup (url);
  connection->proxy_host = g_strdup (proxy_host);
  connection->proxy_port = proxy_port;
  connection->cb_func = cb_func;
  connection->cb_user_data = user_data;
  connection->status = STATUS_NOT_EXECUTED;
  connection->received = NULL;
  connection->fd = 0;
  connection->counter = 1;

  /* start idle function */
  connection->id = g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, weather_http_receive_data_idle,
                                    connection, weather_http_receive_data_destroyed);

  /* add the idle function to the running tasks list */
  qeued_transfers = g_slist_prepend (qeued_transfers, connection);
}



void
weather_http_cleanup_qeue (void)
{
  GSList            *li;
  WeatherConnection *connection;

  for (li = qeued_transfers; li; li = li->next)
    {
      connection = li->data;

      if (connection->status == STATUS_RUNNING)
        {
          /* change status */
          connection->status = STATUS_LEAVE_IMMEDIATELY;
          connection->cb_func = NULL;
        }
      else
        {
          /* remove timeout */
          g_source_remove (connection->id);
        }
    }
}
