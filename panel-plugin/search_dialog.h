#include "http_client.h"

#include <libxml/parser.h>
#include <gtk/gtk.h>
#include <libxfcegui4/dialogs.h>
#include <libxfcegui4/xfce_framebox.h>

struct search_dialog {
        GtkWidget *dialog;
        GtkWidget *search_entry;
        GtkWidget *result_list;
        GtkListStore *result_mdl;

        gchar *result;

        gchar *proxy_host;
        gint proxy_port;
};

struct search_dialog *create_search_dialog(GtkWindow *, gchar *, gint);
gboolean run_search_dialog(struct search_dialog *dialog);
gchar *search_dialog_result(struct search_dialog *dialog);
void free_search_dialog(struct search_dialog *dialog);
