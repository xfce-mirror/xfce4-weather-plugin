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

#ifndef SEARCH_DIALOG_H
#define SEARCH_DIALOG_H

G_BEGIN_DECLS

typedef struct
{
        GtkWidget *dialog;
        GtkWidget *search_entry;
        GtkWidget *result_list;
        GtkListStore *result_mdl;

        gchar *result;

        gchar *proxy_host;
        gint proxy_port;

        gchar *recv_buffer;
}
search_dialog;

search_dialog *
create_search_dialog (GtkWindow *, gchar *, gint);

gboolean
run_search_dialog    (search_dialog *dialog);

void
free_search_dialog   (search_dialog *dialog);

G_END_DECLS

#endif
