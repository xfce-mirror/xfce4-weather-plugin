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

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <resolv.h>
#include <signal.h>
#include <setjmp.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfce4panel/libxfce4panel.h>

#include "weather-http.h"



#define WEATHER_MAX_CONN_TIMEOUT   (10)        /* connection timeout in seconds */
#define WEATHER_RESCHEDULE_TIMEOUT (30 * 1000) /* reschedule timeout in ms */
#define WEATHER_RESCHEDULE_N_TIMES (5)         /* maximum number or reschedule tries */



/* global */
static GSList *queued_transfers = NULL;

enum {
    STATUS_NOT_EXECUTED,
    STATUS_RUNNING,
    STATUS_SUCCEED,
    STATUS_RESCHEDULE,
    STATUS_ERROR,
    STATUS_LEAVE_IMMEDIATELY,
    STATUS_TIMEOUT
};

typedef struct _WeatherConnection WeatherConnection;
struct _WeatherConnection {
    /* thread id */
    gint id;

    /* reschedule counter */
    guint counter;

    /* connection data */
    gchar *hostname;
    gchar *url;
    gchar *proxy_host;
    gint proxy_port;

    /* receive status */
    gint status;

    /* received data */
    gchar *received;
    size_t received_len;

    /* connection descriptor */
    gint fd;

    /* callback data */
    WeatherFunc cb_func;
    gpointer cb_user_data;
};


static gboolean
weather_http_receive_data_check(WeatherConnection *connection,
                                GTimeVal timeout)
{
    GTimeVal now;

    /* check if we need to leave */
    if (G_UNLIKELY(connection->status == STATUS_LEAVE_IMMEDIATELY))
        return TRUE;

    /* get the current time */
    g_get_current_time(&now);

    /* check if we timed out */
    if (G_UNLIKELY((gint) timeout.tv_sec +
                   WEATHER_MAX_CONN_TIMEOUT < (gint) now.tv_sec)) {
        /* set status */
        connection->status = STATUS_TIMEOUT;

        return TRUE;
    }

    return FALSE;
}


static void
refresh_resolvers(void)
{
#ifdef G_OS_UNIX
    res_init();
#endif /*G_OS_UNIX*/
}


#ifdef G_OS_UNIX
static sigjmp_buf jmpenv;

G_GNUC_NORETURN static void
timeout_handler(gint sig)
{
    siglongjmp(jmpenv, 1);
}
#endif /*G_OS_UNIX*/


static gboolean
weather_http_receive_data_idle(gpointer user_data)
{
    WeatherConnection *connection = user_data;
    struct timeval select_timeout;
    struct addrinfo h, *r, *a;
    const gchar *p;
    gchar buffer[1024], *request, *port = NULL;
    gint bytes = 0, n, m = 0, err;
    fd_set fds;
    GTimeVal timeout;

#ifdef G_OS_UNIX
    void (*prev_handler) (gint);

    alarm(0);
    prev_handler = signal(SIGALRM, timeout_handler);
    if (sigsetjmp(jmpenv, 1)) {
        alarm(0);
        signal(SIGALRM, prev_handler);
        connection->status = STATUS_TIMEOUT;
        return FALSE;
    }
#endif
    /* set the current time */
    g_get_current_time(&timeout);

    /* force the libc to get resolvers right, if they changed for some reason */
    refresh_resolvers();

    /* try to get the hostname */
#ifdef G_OS_UNIX
    alarm(WEATHER_MAX_CONN_TIMEOUT);
#endif

    memset(&h, 0, sizeof(h));
    h.ai_family = AF_UNSPEC;
    h.ai_socktype = SOCK_STREAM;
    h.ai_protocol = IPPROTO_TCP;

    if (connection->proxy_port)
        port = g_strdup_printf("%d", connection->proxy_port);
    else
        port = g_strdup("80");

    err = getaddrinfo(connection->proxy_host
                      ? connection->proxy_host : connection->hostname,
                      port, &h, &r);

    g_free(port);
#ifdef G_OS_UNIX
    alarm(0);
    signal(SIGALRM, prev_handler);
#endif

    if (G_UNLIKELY(err != 0)) {
        /* display error */
        g_message(_("Failed to get the hostname %s. Retry in %d seconds."),
                  gai_strerror(err),
                  WEATHER_RESCHEDULE_TIMEOUT / 1000);

        /* try again later */
        connection->status = STATUS_RESCHEDULE;

        return FALSE;
    }

    if (weather_http_receive_data_check(connection, timeout))
        return FALSE;

    /* open the socket */

    for (a = r; a != NULL; a = a->ai_next) {
        connection->fd = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (connection->fd < 0)
            continue;

#ifdef G_OS_UNIX
        signal(SIGALRM, timeout_handler);
        alarm(WEATHER_MAX_CONN_TIMEOUT);
#endif

        m = connect(connection->fd, a->ai_addr, a->ai_addrlen);

#ifdef G_OS_UNIX
        alarm(0);
        signal(SIGALRM, prev_handler);
#endif

        if (m == 0)
            break;

        if (weather_http_receive_data_check(connection, timeout))
            break;
    }

    if (G_UNLIKELY(connection->fd < 0)) {
        g_warning(_("Failed to open the socket(%s)."), g_strerror(errno));
        connection->status = STATUS_ERROR;
        return FALSE;
    }

    if (G_UNLIKELY(m < 0)) {
        g_warning(_("Failed to create a connection with the host(%s)."),
                  g_strerror(errno));
        connection->status = STATUS_ERROR;
        return FALSE;
    }

    if (weather_http_receive_data_check(connection, timeout))
        return FALSE;

    /* create the request */
    if (connection->proxy_host)
        request = g_strdup_printf("GET http://%s%s HTTP/1.0\r\n\r\n",
                                  connection->hostname, connection->url);
    else
        request = g_strdup_printf("GET %s HTTP/1.%d\r\n"
                                  "Host: %s\r\n"
                                  "Connection: close\r\n"
                                  "\r\n",
                                  connection->url,
                                  strcmp(connection->hostname, "geoip.xfce.org")
                                  ? 1 : 0,
                                  connection->hostname);

    /* send the request */
    for (m = 0, n = strlen(request); m < n;) {
        /* send some info to the host */
        bytes = send(connection->fd, request + m, n - m, 0);

        if (weather_http_receive_data_check(connection, timeout)) {
            g_free(request);
            return FALSE;
        }

        /* check for problems */
        if (G_UNLIKELY(bytes < 0)) {
            /* just try again on EAGAIN/EINTR */
            if (G_LIKELY(errno != EAGAIN && errno != EINTR)) {
                g_warning(_("Failed to send the request(%s)."),
                          g_strerror(errno));
                g_free(request);
                connection->status = STATUS_ERROR;
                return FALSE;
            }
        } else {
            /* advance the offset */
            m += bytes;
        }
    }

    /* cleanup the request */
    g_free(request);

    /* create an empty string */
    connection->received_len = 0;

    /* download the file content */
    FD_ZERO(&fds);
    do {
        /*
         * FIXME: recv() may block. send() and connect() may block too and the
         * only right solution is to rewrite whole code using non-blocking
         * sockets, but that's hard, connect() is already protected with alarm()
         * and send() blocks only when socket buffers are ultra-small
         */
        FD_SET(connection->fd, &fds);
        select_timeout.tv_sec = WEATHER_MAX_CONN_TIMEOUT;
        select_timeout.tv_usec = 0;

        m = select(connection->fd+1, &fds, 0, 0, &select_timeout);
        if (G_LIKELY(m == 1)) {
            bytes = recv(connection->fd, buffer,
                         sizeof(buffer) - sizeof(gchar), 0);
            if (G_LIKELY(bytes > 0)) {
                /* prepend the downloaded data */
                connection->received =
                    g_realloc(connection->received,
                              connection->received_len + bytes);
                memcpy(connection->received + connection->received_len,
                       buffer, bytes);
                connection->received_len += bytes;
            }
        }

        /* check for problems */
        if (G_UNLIKELY(m < 0 || bytes < 0)) {
            g_warning(_("Failed to receive data(%s)"), g_strerror(errno));
            connection->status = STATUS_ERROR;
            return FALSE;
        }

        if (weather_http_receive_data_check(connection, timeout))
            return FALSE;
    } while(bytes > 0);

    if (G_LIKELY(connection->received_len > 0)) {
        /* get the pointer to the content-length */
        int cts_len = -1;
        p = strstr((char *) connection->received, "Content-Length:");
        if (G_LIKELY(p)) {
            /* advance the pointer */
            p += strlen("Content-Length:");

            cts_len = strtol(p, NULL, 10);
            if (G_UNLIKELY(cts_len < 0)) {
                g_warning(_("Negative content length"));
                connection->status = STATUS_ERROR;
            }
        } else {
            /* hack for geoip.xfce.org, which return no content-length */
            p = strstr((char *)connection->received, "<Response>");
            if (G_LIKELY(p))
                cts_len = connection->received_len - (p - connection->received);
        }

        if (cts_len > -1) {
            /* calculate the header length */
            n = connection->received_len - cts_len;

            if (G_LIKELY(n > 0)) {
                /* erase the header from the received string */
                void *tmp = g_malloc(cts_len+1);
                memcpy(tmp, connection->received+n, cts_len);
                ((gchar *) tmp)[cts_len] = 0;
                g_free(connection->received);
                connection->received = tmp;
                connection->received_len = cts_len;
            }

            connection->status = STATUS_SUCCEED;
        } else {
            g_warning(_("Unable to detect the content length."));
            connection->status = STATUS_ERROR;
        }
    } else {
        g_warning(_("No content received."));
        connection->status = STATUS_ERROR;
    }
    return FALSE;
}


static void
weather_http_receive_data_destroyed(gpointer user_data)
{
    WeatherConnection *connection = user_data;

    /* close the socket */
    if (connection->fd >= 0) {
        close(connection->fd);
        connection->fd = -1;
    }

    if (connection->status == STATUS_TIMEOUT)
        g_message("Connection timeout");

    if (connection->status == STATUS_SUCCEED && connection->cb_func) {
        /* execute the callback process */
        (*connection->cb_func) (TRUE,
                                connection->received,
                                connection->received_len,
                                connection->cb_user_data);
    } else if (connection->status == STATUS_RESCHEDULE &&
               connection->counter < WEATHER_RESCHEDULE_N_TIMES) {
        /* cleanup the received data */
        if (connection->received) {
            g_free(connection->received);
            connection->received = NULL;
        }

        /* increase counter */
        connection->counter++;

        /* reschedule request */
        connection->id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
                                            WEATHER_RESCHEDULE_TIMEOUT,
                                            weather_http_receive_data_idle,
                                            connection,
                                            weather_http_receive_data_destroyed);

        /* leave without freeing data */
        return;
    } else {
        /* execute empty callback */
        if (connection->cb_func)
            (*connection->cb_func) (FALSE, NULL, 0, connection->cb_user_data);

        /* cleanup */
        if (connection->received) {
            g_free(connection->received);
            connection->received = NULL;
        }
    }

    /* remove from the list */
    queued_transfers = g_slist_remove(queued_transfers, connection);

    /* free other data */
    g_free(connection->hostname);
    g_free(connection->url);
    g_free(connection->proxy_host);

    /* cleanup */
    g_slice_free(WeatherConnection, connection);
}



void
weather_http_receive_data(const gchar *hostname,
                          const gchar *url,
                          const gchar *proxy_host,
                          gint proxy_port,
                          WeatherFunc cb_func,
                          gpointer user_data)
{
    WeatherConnection *connection;

    /* create slice */
    connection = g_slice_new0(WeatherConnection);

    /* set connection properties */
    connection->hostname = g_strdup(hostname);
    connection->url = g_strdup(url);
    connection->proxy_host = g_strdup(proxy_host);
    connection->proxy_port = proxy_port;
    connection->cb_func = cb_func;
    connection->cb_user_data = user_data;
    connection->status = STATUS_NOT_EXECUTED;
    connection->received = NULL;
    connection->received_len = 0;
    connection->fd = -1;
    connection->counter = 1;

    /* start idle function */
    connection->id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                     weather_http_receive_data_idle,
                                     connection,
                                     weather_http_receive_data_destroyed);

    /* add the idle function to the running tasks list */
    queued_transfers = g_slist_prepend(queued_transfers, connection);
}


void
weather_http_cleanup_queue(void)
{
    GSList *li;
    WeatherConnection *connection;

    for (li = queued_transfers; li; li = li->next) {
        connection = li->data;

        if (connection->status == STATUS_RUNNING) {
            /* change status */
            connection->status = STATUS_LEAVE_IMMEDIATELY;
            connection->cb_func = NULL;
        } else {
            /* remove timeout */
            g_source_remove(connection->id);
        }
    }
}
