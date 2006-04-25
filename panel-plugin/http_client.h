#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>

#include <glib.h>

typedef void(*CB_TYPE)(gboolean, gpointer);

gboolean http_get_file(gchar *url, gchar *hostname, 
                gchar *filename, gchar *proxy_host, gint proxy_port, 
                CB_TYPE callback, gpointer data);
gboolean http_get_buffer(gchar *url, gchar *hostname, 
                gchar *proxy_host, gint proxy_port, gchar **buffer,
                CB_TYPE callback, gpointer data);
#endif
