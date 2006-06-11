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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <fcntl.h>
#include <errno.h>

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include <stdio.h>

#include <glib.h>
#include <gmodule.h>

#include "http_client.h"

struct request_data 
{
    int    fd;
    
    FILE      *save_fp;
    gchar     *save_filename;
    gchar    **save_buffer;

    gboolean   has_header;
    gchar      last_chars[4];

    gchar     *request_buffer;
    gint       offset;
    gint       size;

    CB_TYPE    cb_function;
    gpointer   cb_data;
};

static gboolean
keep_receiving (gpointer data);


static int
http_connect (gchar *hostname,
          gint   port)
{
    struct sockaddr_in dest_host;
    struct hostent *host_address;
    gint fd;
           
    if ((host_address = gethostbyname(hostname)) == NULL)
        return -1;

    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        return -1;
       
    dest_host.sin_family = AF_INET;
    dest_host.sin_port = htons(port);
    dest_host.sin_addr = *((struct in_addr *)host_address->h_addr);
    memset(&(dest_host.sin_zero), '\0', 8);
    fcntl(fd, F_SETFL, O_NONBLOCK);
    
    if ((connect(fd, (struct sockaddr *)&dest_host, sizeof(struct sockaddr)) == -1)
            && errno != EINPROGRESS)
    {
        perror("http_connect()");
        return -1;
    }
    else
        return fd;
    
}
       
static void
request_free (struct request_data *request)
{
    if (request->request_buffer)
        g_free(request->request_buffer);

    if (request->save_filename)
        g_free(request->save_filename);

    if (request->save_fp)
        fclose(request->save_fp);

    if (request->fd)
        close(request->fd);

    g_free(request);
}

static void
request_save (struct request_data *request,
          const gchar     *buffer)
{
    //DBG ("Request Save");

    if (request->save_filename)
        if (!request->save_fp && 
                (request->save_fp = fopen(request->save_filename, "w"))
                == NULL)
            return;
        else
            fwrite(buffer, sizeof(char), strlen(buffer), 
                    request->save_fp);
    else
    {
        if (*request->save_buffer)
        {
            gchar *newbuff = g_strconcat(*request->save_buffer,
                    buffer, NULL);
            g_free(*request->save_buffer);
            *request->save_buffer = newbuff;
        }
        else
            *request->save_buffer = g_strdup(buffer);
    }
}               
        
static gboolean
keep_sending (gpointer data)
{
    struct request_data *request = (struct request_data *)data;
    gint n;

    if (!request)
    {
        //DBG ("keep_sending(): empty request data");
        return FALSE;
    }
    
    if ((n = send(request->fd, request->request_buffer + request->offset, 
                    request->size - request->offset, 
                    0)) != -1)
    {
        request->offset += n;

        //DBG ("now at offset: %d", request->offset);
        //DBG ("now at byte: %d", n);
        
        if (request->offset == request->size)
        {
            //DBG ("keep_sending(): ok data sent");
            g_idle_add(keep_receiving, (gpointer) request);
            return FALSE;
        }
    }
    else if (errno != EAGAIN) /* some other error happened */
    {
#if DEBUG
        perror("keep_sending()");
        //DBG ("file desc: %d", request->fd);
#endif          
        request_free(request);
        return FALSE;
    }

    return TRUE;
} 

static gboolean
keep_receiving (gpointer data)
{
    struct request_data *request = (struct request_data *)data;
    gchar recvbuffer[1024];
    gint n;
    gchar *p;
    gchar *str = NULL;
    
    if (!request)
    {
        //DBG ("keep_receiving(): empty request data ");
        return FALSE;
    }

    if ((n = recv(request->fd, recvbuffer, sizeof(recvbuffer) - 
                    sizeof(char), 0)) > 0)
    {
        recvbuffer[n] = '\0';

        //DBG ("keep_receiving(): bytes recv: %d", n);
        
        if (!request->has_header)
        {
            if (request->last_chars != '\0')
                str = g_strconcat(request->last_chars, 
                        recvbuffer, NULL);
            
            if ((p = strstr(str, "\r\n\r\n")))
            {
                request_save(request, p + 4);
                request->has_header = TRUE;
                //DBG ("keep_receiving(): got header");
            }
            else
            {
                //DBG ("keep_receiving(): no header yet\n\n%s\n..\n",
                //        recvbuffer);      
                memcpy(request->last_chars, recvbuffer + (n - 4), 
                        sizeof(char) * 3);
            }

            g_free(str);
        }
        else
            request_save(request, recvbuffer);
    }
    else if (n == 0)
    {
        CB_TYPE callback = request->cb_function;
        gpointer data = request->cb_data;
        //DBG ("keep_receiving(): ending with succes");
        request_free(request);

        callback(TRUE, data);
        return FALSE;
    }
    else if (errno != EAGAIN)
    {
        perror("keep_receiving()");
        request->cb_function(FALSE, request->cb_data);
        request_free(request);
        return FALSE;
    }

    return TRUE;
}
                
                

static gboolean
http_get (gchar     *url,
      gchar     *hostname,
      gboolean   savefile,
      gchar    **fname_buff, 
      gchar     *proxy_host,
      gint       proxy_port,
      CB_TYPE    callback,
      gpointer   data)
{
    struct request_data *request = g_new0(struct request_data, 1);

    if (!request)
    {
#if DEBUG
        perror("http_get(): empty request");
#endif
        return FALSE;
    }
    
    request->has_header = FALSE;
    request->cb_function = callback;
    request->cb_data = data;

    if (proxy_host)
    {
        //DBG ("using proxy %s", proxy_host);
        request->fd = http_connect(proxy_host, proxy_port);
    }
    else
    {
        //DBG ("Not USING PROXY");
        request->fd = http_connect(hostname, 80);
    }

    if (request->fd == -1)
    {
        //DBG ("http_get(): fd = -1 returned");
        request_free(request);
        return FALSE;
    }
    
    if (proxy_host)
        request->request_buffer = g_strdup_printf(
                "GET http://%s%s HTTP/1.0\r\n\r\n",
                hostname, url);
    else
        request->request_buffer = g_strdup_printf("GET %s HTTP/1.0\r\n"
                "Host: %s\r\n\r\n", url, hostname);

    if (request->request_buffer == NULL)
    {
#if DEBUG
        perror("http_get(): empty request buffer\n");
#endif
        close(request->fd);
        g_free(request);
        return FALSE;
    }

    request->size = strlen (request->request_buffer);

    if (savefile)
        request->save_filename = g_strdup (*fname_buff);
    else
        request->save_buffer = fname_buff;

    //DBG ("http_get(): adding idle function");
    
    (void)g_idle_add ((GSourceFunc)keep_sending, (gpointer)request);

    //DBG ("http_get(): request added");

    return TRUE;
}    

gboolean
http_get_file (gchar   *url,
               gchar   *hostname,
               gchar   *filename, 
               gchar   *proxy_host,
               gint     proxy_port,
               CB_TYPE  callback,
               gpointer data)
{
    return http_get (url, hostname, TRUE, &filename, proxy_host, proxy_port, 
            callback, data);
}

gboolean
http_get_buffer (gchar    *url,
                 gchar    *hostname,
                 gchar    *proxy_host, 
                 gint      proxy_port,
                 gchar   **buffer,
                 CB_TYPE   callback,
                 gpointer  data)
{  
    return http_get (url, hostname, FALSE, buffer, proxy_host, proxy_port, 
            callback, data);
}
