#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>

#include <stdio.h>

#include <glib.h>

gboolean http_get_file(gchar *url, gchar *hostname, gchar *filename);
gchar *http_get_buffer(gchar *url, gchar *hostname);
