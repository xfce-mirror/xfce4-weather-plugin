#include "http_client.h"

#include <config.h>

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
};

struct search_dialog *create_search_dialog(GtkWindow *);
gboolean run_search_dialog(struct search_dialog *dialog);
gchar *search_dialog_result(struct search_dialog *dialog);
