#include <config.h>
#include <fcntl.h>
#include <errno.h>
#include "http_client.h"
#include "debug_print.h"

struct request_data 
{
        int fd;
        
        FILE *save_fp;
        gchar *save_filename;
        gchar **save_buffer;

        gboolean has_header;
        gchar last_chars[4];

        gchar *request_buffer;
        int offset;
        int size;

        CB_TYPE cb_function;
        gpointer cb_data;
};

gboolean keep_receiving(gpointer data);


int http_connect(gchar *hostname, gint port)
{
        struct sockaddr_in dest_host;
        struct hostent *host_address;
        int fd;
               
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
       
void request_free(struct request_data *request)
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

void request_save(struct request_data *request, const gchar *buffer)
{
        DEBUG_PUTS("request_save(): get save request\n");
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
                                        buffer);
                        g_free(*request->save_buffer);
                        *request->save_buffer = newbuff;
                }
                else
                        *request->save_buffer = g_strdup(buffer);
        }
}                       
                
gboolean keep_sending(gpointer data)
{
        struct request_data *request = (struct request_data *)data;
        int n;

        if (!request)
        {
                DEBUG_PUTS("keep_sending(): empty request data\n");
                return FALSE;
        }
        
        if ((n = send(request->fd, request->request_buffer + request->offset, 
                                        request->size - request->offset, 
                                        0)) != -1)
        {
                request->offset += n;

                DEBUG_PRINT("now at offset: %d\n", request->offset);
                DEBUG_PRINT("now at byte: %d\n", n);
                
                if (request->offset == request->size)
                {
                        DEBUG_PUTS("keep_sending(): ok data sent\n");
                        g_idle_add(keep_receiving, (gpointer) request);
                        return FALSE;
                }
        }
        else if (errno != EAGAIN) /* some other error happened */
        {
#if DEBUG
                perror("keep_sending()");
                DEBUG_PRINT("file desc: %d\n", request->fd);
#endif              
                request_free(request);
                return FALSE;
        }

        return TRUE;
} 

gboolean keep_receiving(gpointer data)
{
        struct request_data *request = (struct request_data *)data;
        char recvbuffer[1024];
        int n;
        gchar *p;
        
        if (!request)
        {
                DEBUG_PUTS("keep_receiving(): empty request data\n");
                return FALSE;
        }

        if ((n = recv(request->fd, recvbuffer, sizeof(recvbuffer) - 
                                        sizeof(char), 0)) > 0)
        {
                recvbuffer[n] = '\0';

                DEBUG_PRINT("keep_receiving(): bytes recv: %d\n", n);
                
                if (!request->has_header)
                {
                        gchar *str;

                        if (request->last_chars != '\0')
                                str = g_strconcat(request->last_chars, 
                                                recvbuffer, NULL);
                        
                        if ((p = strstr(str, "\r\n\r\n")))
                        {
                                request_save(request, p + 4);
                                request->has_header = TRUE;
                                DEBUG_PUTS("keep_receiving(): got header");
                        }
                        else
                        {
                                DEBUG_PRINT("keep_receiving(): no header yet\n\n%s\n..\n",
                                                recvbuffer);          
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
                DEBUG_PUTS("keep_receiving(): ending with succes\n");
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
                                
                                

gboolean http_get(gchar *url, gchar *hostname, gboolean savefile, gchar **fname_buff, 
                gchar *proxy_host, gint proxy_port, CB_TYPE callback, gpointer data)
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
                DEBUG_PRINT("using proxy %s\n", proxy_host);
                request->fd = http_connect(proxy_host, proxy_port);
        }
        else
        {
                DEBUG_PUTS("Not USING PROXY\n");
                request->fd = http_connect(hostname, 80);
        }

        if (request->fd == -1)
        {
                DEBUG_PUTS("http_get(): fd = -1 returned\n");
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

        request->size = strlen(request->request_buffer);

        if (savefile)
                request->save_filename = g_strdup(*fname_buff);
        else
                request->save_buffer = fname_buff;

        DEBUG_PUTS("http_get(): adding idle function");
        
        (void)g_idle_add((GSourceFunc)keep_sending, (gpointer)request);

        DEBUG_PUTS("http_get(): request added\n");

        return TRUE;
}        

gboolean http_get_file(gchar *url, gchar *hostname, gchar *filename, 
                gchar *proxy_host, gint proxy_port, CB_TYPE callback, gpointer data)
{
        return http_get(url, hostname, TRUE, &filename, proxy_host, proxy_port, 
                        callback, data);
}

gboolean http_get_buffer(gchar *url, gchar *hostname, gchar *proxy_host, 
                gint proxy_port, gchar **buffer, CB_TYPE callback, gpointer data)
{  
        return http_get(url, hostname, FALSE, buffer, proxy_host, proxy_port, 
                        callback, data);
}
