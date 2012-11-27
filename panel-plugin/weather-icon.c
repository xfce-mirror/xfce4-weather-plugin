/*  Copyright (c) 2003-2012 Xfce Development Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include <libxfce4util/libxfce4util.h>

#include "weather-icon.h"
#include "weather-debug.h"

#define DEFAULT_W_THEME "liquid"
#define THEME_INFO_FILE "theme.info"
#define ICON_DIR_SMALL "22"
#define ICON_DIR_MEDIUM "48"
#define ICON_DIR_BIG "128"
#define NODATA "NODATA"


GdkPixbuf *
get_icon(const icon_theme *theme,
         const gchar *number,
         const gint size,
         const gboolean night)
{
    GdkPixbuf *image = NULL;
    const gchar* dir;
    gchar *filename, *suffix = "";
    guint i;

    g_assert(theme != NULL);
    if (G_UNLIKELY(!theme)) {
        g_warning("No icon theme!");
        return NULL;
    }

    /* Try to use optimal size */
    if (size < 24)
        dir = ICON_DIR_SMALL;
    else if (size < 49)
        dir = ICON_DIR_MEDIUM;
    else
        dir = ICON_DIR_BIG;

    if (number == NULL || strlen(number) == 0)
        number = NODATA;
    else if (night)
        suffix = "-night";

    filename = g_strconcat(theme->dir, G_DIR_SEPARATOR_S,
                           dir, G_DIR_SEPARATOR_S,
                           g_ascii_strdown(number, -1),
                           suffix, ".png", NULL);

    image = gdk_pixbuf_new_from_file_at_scale(filename, size, size, TRUE, NULL);

    if (G_UNLIKELY(!image)) {
        weather_debug("Unable to open image: %s", filename);
        if (number && strcmp(number, NODATA)) {
            g_free(filename);

            /* maybe there is no night icon, so fallback to using day icon */
            if (night)
                return get_icon(theme, number, size, FALSE);
            else
                return get_icon(theme, NULL, size, FALSE);
        } else {
            /* last fallback: get NODATA icon from standard theme */
            g_free(filename);
            filename = g_strconcat(THEMESDIR, G_DIR_SEPARATOR_S,
                                   DEFAULT_W_THEME, G_DIR_SEPARATOR_S,
                                   dir, G_DIR_SEPARATOR_S,
                                   g_ascii_strdown(NODATA, -1), ".png", NULL);
            image = gdk_pixbuf_new_from_file_at_scale(filename, size, size,
                                                      TRUE, NULL);
            if (G_UNLIKELY(!image))
                g_warning("Failed to open image: %s", filename);
        }
    }
    g_free(filename);

    return image;
}


/*
 * Load icon theme info from theme info file given a directory.
 */
icon_theme *
icon_theme_load_info(const gchar *dir)
{
    XfceRc *rc;
    icon_theme *theme = NULL;
    gchar *filename;
    const gchar *value;

    g_assert(dir != NULL);
    if (G_UNLIKELY(dir == NULL))
        return NULL;

    filename = g_build_filename(dir, G_DIR_SEPARATOR_S, THEME_INFO_FILE, NULL);

    if (g_file_test(filename, G_FILE_TEST_EXISTS)) {
        rc = xfce_rc_simple_open(filename, TRUE);
        g_free(filename);
        filename = NULL;

        if (!rc)
            return NULL;

        if ((theme = g_slice_new0(icon_theme)) == NULL) {
            xfce_rc_close(rc);
            return NULL;
        }

        theme->dir = g_strdup(dir);

        value = xfce_rc_read_entry(rc, "Name", NULL);
        if (value)
            theme->name = g_strdup(value);
        else {
            /* Use directory name as fallback */
            filename = g_path_get_dirname(dir);
            if (G_LIKELY(strcmp(filename, "."))) {
                theme->dir = g_strdup(dir);
                theme->name = g_strdup(filename);
                weather_debug("No Name found in theme info file, "
                              "using directory name %s as fallback.", dir);
                g_free(filename);
                filename = NULL;
            } else { /* some weird error, not safe to proceed */
                weather_debug("Some weird error, not safe to proceed. "
                              "Abort loading icon theme from %s.", dir);
                icon_theme_free(theme);
                g_free(filename);
                xfce_rc_close(rc);
                return NULL;
            }
        }

        value = xfce_rc_read_entry(rc, "Author", NULL);
        if (value)
            theme->author = g_strdup(value);

        value = xfce_rc_read_entry(rc, "Description", NULL);
        if (value)
            theme->description = g_strdup(value);

        value = xfce_rc_read_entry(rc, "License", NULL);
        if (value)
            theme->license = g_strdup(value);
        xfce_rc_close(rc);
    }

    weather_dump(weather_dump_icon_theme, theme);
    return theme;
}


/*
 * Load theme from a directory, fallback to standard theme on failure
 * or when dir is NULL.
 */
icon_theme *
icon_theme_load(const gchar *dir)
{
    icon_theme *theme = NULL;
    gchar *default_dir;

    if (dir != NULL) {
        weather_debug("Loading icon theme from %s.", dir);
        if ((theme = icon_theme_load_info(dir)) != NULL) {
            weather_debug("Successfully loaded theme from %s.", dir);
            return theme;
        } else
            weather_debug("Error loading theme from %s.", dir);
    }

    /* on failure try the standard theme */
    if (theme == NULL) {
        default_dir = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s",
                                      THEMESDIR, DEFAULT_W_THEME);
        weather_debug("Loading standard icon theme from %s.", default_dir);
        if ((theme = icon_theme_load_info(default_dir)) != NULL)
            weather_debug("Successfully loaded theme from %s.", default_dir);
        else
            weather_debug("Error loading standard theme from %s.", default_dir);
        g_free(default_dir);
    }
    return theme;
}


static GArray *
find_themes_in_dir(const gchar *path)
{
    GArray *themes = NULL;
    GDir *dir;
    icon_theme *theme;
    gchar *themedir;
    const gchar *dirname;

    g_assert(path != NULL);
    if (G_UNLIKELY(path == NULL))
        return NULL;

    weather_debug("Looking for icon themes in %s.", path);
    dir = g_dir_open(path, 0, NULL);
    if (dir) {
        themes = g_array_new(FALSE, TRUE, sizeof(icon_theme*));

        while (dirname = g_dir_read_name(dir)) {
            themedir = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s",
                                       path, dirname);
            theme = icon_theme_load_info(themedir);
            g_free(themedir);

            if (theme) {
                themes = g_array_append_val(themes, theme);
                weather_debug("Found icon theme %s", theme->dir);
                weather_dump(weather_dump_icon_theme, theme);
            }
        }
        g_dir_close(dir);
    } else
        weather_debug("Could not list directory %s.", path);
    weather_debug("Found %d icon theme(s) in %s.", themes->len, path);
    return themes;
}


/*
 * Find all available themes in user's config dir and at the install
 * location.
 */
GArray *
find_icon_themes(void)
{
    GArray *themes, *found;
    gchar *dir;

    themes = g_array_new(FALSE, TRUE, sizeof(icon_theme *));

    /* look in user directory first */
    dir = g_strconcat(g_get_user_config_dir(), G_DIR_SEPARATOR_S,
                      "xfce4", G_DIR_SEPARATOR_S, "weather",
                      G_DIR_SEPARATOR_S, "icons", NULL);
    found = find_themes_in_dir(dir);

    g_free(dir);
    if (found->len > 0)
        themes = g_array_append_vals(themes, found->data, found->len);
    g_array_free(found, FALSE);

    /* next find themes in system directory */
    found = find_themes_in_dir(THEMESDIR);
    if (found->len > 0)
        themes = g_array_append_vals(themes, found->data, found->len);
    g_array_free(found, FALSE);

    weather_debug("Found %d icon themes in total.", themes->len, dir);
    return themes;
}


icon_theme *
icon_theme_copy(icon_theme *src)
{
    icon_theme *dst;

    if (G_UNLIKELY(src == NULL))
        return NULL;

    dst = g_slice_new0(icon_theme);
    if (G_UNLIKELY(dst == NULL))
        return NULL;

    if (src->dir)
        dst->dir = g_strdup(src->dir);
    if (src->name)
        dst->name = g_strdup(src->name);
    if (src->author)
        dst->author = g_strdup(src->author);
    if (src->description)
        dst->description = g_strdup(src->description);
    if (src->license)
        dst->license = g_strdup(src->license);
    return dst;
}


void
icon_theme_free(icon_theme *theme)
{
    g_assert(theme != NULL);
    if (G_UNLIKELY(theme == NULL))
        return;
    g_free(theme->dir);
    g_free(theme->name);
    g_free(theme->author);
    g_free(theme->description);
    g_free(theme->license);
    g_slice_free(icon_theme, theme);
}
