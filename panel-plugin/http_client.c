#include <config.h>

#include "http_client.h"
#include "debug_print.h"

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
        dest_host.sin_addr = *((struct in_addr *)host_address->h_addr);
        dest_host.sin_port = htons(port);
        memset(&(dest_host.sin_zero), '\0', 8);

        if (connect(fd, (struct sockaddr *)&dest_host, sizeof(struct sockaddr)) == -1)
        {
                close(fd);
                return -1;
        }
       
        /* TODO fcntl(fd, F_SETFL, O_NONBLOCK); */

        return fd;
}
       
int http_recv(int fd, gchar **buffer)
{
        int n = 0; /* 1 = good, 0 = conn terminated, -1 = error */
        gchar thisbuffer[1024]; 
        
        n = recv(fd, thisbuffer, 1023, 0);

        

        if (n == -1)
        {
                *buffer = NULL;
        }
        else if (n == 0) {
                *buffer = NULL;
        }
        else {
                
                thisbuffer[n] = '\0';
                *buffer = g_strdup((const gchar *)thisbuffer); 
        }

        

        return n;
}
                
gboolean http_get_header(int fd, gchar **buffer)
{
        gchar lastchar = 0, *thisbuffer;
        int l;
        
        while((l = http_recv(fd, &thisbuffer)) > 0)
        {
                gboolean found = FALSE;
                gchar *where;
                gchar *p;

                
                
                if (lastchar == '\r' &&
                                (p = g_strstr_len(thisbuffer, 3, "\n\r\n"))) {
                        
                        where = p + 3;
                        found = TRUE;
                        
                }
                else if ((p = strstr(thisbuffer, "\r\n\r\n"))) {
                        where = p + 4;
                        found = TRUE;
                }

                if (found)
                {
                        /*TODO check if at end*/
                        *buffer = g_strdup(where);
                }
                else
                        lastchar = thisbuffer[l];
                
                g_free(thisbuffer);

                if (found) 
                        return TRUE;
        }

        return FALSE;
}

gboolean http_get(gchar *url, gchar *hostname, gboolean savefile, gchar **fname_buff, 
                gchar *proxy_host, gint proxy_port)
{
        int fd, error;
        FILE *file = NULL;
        gchar *buffer = NULL;
        gchar *retstr = NULL;
        gchar *request = NULL;

        if (proxy_host)
                fd = http_connect(proxy_host, proxy_port);
        else
                fd = http_connect(hostname, 80);
        
        if (fd == -1)
                return FALSE;

        if (proxy_host)
                request = g_strdup_printf("GET http://%s%s HTTP/1.0\r\n\r\n",
                                hostname, url);
        else
                request = g_strdup_printf("GET %s HTTP/1.0\r\n"
                                "Host: %s\r\n\r\n", url, hostname);

        if (request == NULL)
        {
                close(fd);
                return FALSE;
        }

        error = send(fd, request, strlen(request), 0);
        g_free(request);

        if (error == -1)
        { 
                close(fd);
                return FALSE;
        }

        if (savefile)
        {
                file = fopen(*fname_buff, "w");

                if (!file)
                {
                       DEBUG_PRINT("Error opening file %s\n", *fname_buff);
                       close(fd);
                        return FALSE;
                }
        }


        if (http_get_header(fd, &buffer) == FALSE)
        {
                close(fd);
                return FALSE;
        }


        if (buffer)
        {
                int l = strlen(buffer);
                
                if (savefile)
                        fwrite(buffer, sizeof(char), l, file);
                else
                        retstr = g_strdup(buffer);

                g_free(buffer);
        }

        while((error = http_recv(fd, &buffer)) > 0)
        {
                
                if (savefile) 
                {
                        int l = strlen(buffer);
                        fwrite(buffer, sizeof(char), l, file);
                }
                else
                {
                        if (retstr) 
                        {
                                gchar *str;
                                str = g_strconcat(retstr, buffer, NULL);
                                g_free(retstr);
                                retstr = str;
                        }
                        else
                                retstr = g_strdup(buffer);
                }
                        

                g_free(buffer);
        }

        if (error == -1)
        {
                fclose(file); /*TODO unlink*/
                close(fd);
                g_free(retstr);
                return FALSE;
        }

        if (savefile)
                fclose(file);
        else
                *fname_buff = retstr;

        close(fd);

        return TRUE;
}

        

gboolean http_get_file(gchar *url, gchar *hostname, gchar *filename, 
                gchar *proxy_host, gint proxy_port)
{
        return http_get(url, hostname, TRUE, &filename, proxy_host, proxy_port);
}

gchar *http_get_buffer(gchar *url, gchar *hostname, gchar *proxy_host, gint proxy_port)
{
        gchar *buffer = NULL;
        
        http_get(url, hostname, FALSE, &buffer, proxy_host, proxy_port);

        return buffer;
}
