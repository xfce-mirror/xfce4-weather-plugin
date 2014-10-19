/*  Copyright (c) 2003-2014 Xfce Development Team
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


static gboolean
icon_missing(const icon_theme *theme,
             const gchar *sizedir,
             const gchar *symbol_name,
             const gchar *suffix)
{
    gchar *missing, *icon;
    gint i;

    icon = g_strconcat(sizedir, G_DIR_SEPARATOR_S, symbol_name, suffix, NULL);
    for (i = 0; i < theme->missing_icons->len; i++) {
        missing = g_array_index(theme->missing_icons, gchar *, i);
        if (G_UNLIKELY(missing == NULL))
            continue;
        if (!strcmp(missing, icon)) {
            g_free(icon);
            return TRUE;
        }
    }
    g_free(icon);
    return FALSE;
}


static void
remember_missing_icon(const icon_theme *theme,
                      const gchar *sizedir,
                      const gchar *symbol_name,
                      const gchar *suffix)
{
    gchar *icon;

    icon = g_strconcat(sizedir, G_DIR_SEPARATOR_S, symbol_name, suffix, NULL);
    g_array_append_val(theme->missing_icons, icon);
    weather_debug("Remembered missing icon %s.", icon);
}


static const gchar *
get_icon_sizedir(const gint size)
{
    const gchar *sizedir;

    if (size < 24)
        sizedir = ICON_DIR_SMALL;
    else if (size < 49)
        sizedir = ICON_DIR_MEDIUM;
    else
        sizedir = ICON_DIR_BIG;
    return sizedir;
}


static gchar *
make_icon_filename(const icon_theme *theme,
                   const gchar *sizedir,
                   const gchar *symbol_name,
                   const gchar *suffix)
{
    gchar *filename, *symlow;

    symlow = g_ascii_strdown(symbol_name, -1);
    filename = g_strconcat(theme->dir, G_DIR_SEPARATOR_S, sizedir,
                           G_DIR_SEPARATOR_S, symlow, suffix, ".png", NULL);
    g_free(symlow);
    return filename;
}


static gchar *
make_fallback_icon_filename(const gchar *sizedir)
{
    gchar *filename, *symlow;

    symlow = g_ascii_strdown(symbol_names[SYMBOL_NODATA], -1);
    filename = g_strconcat(THEMESDIR, G_DIR_SEPARATOR_S, DEFAULT_W_THEME,
                           G_DIR_SEPARATOR_S, sizedir, G_DIR_SEPARATOR_S,
                           symlow, ".png", NULL);
    g_free(symlow);
    return filename;
}


GdkPixbuf *
get_icon(const icon_theme *theme,
         const gchar *symbol_name,
         const gint size,
         const gboolean night)
{
    GdkPixbuf *image = NULL;
    const gchar *sizedir;
    gchar *filename = NULL, *suffix = "";

    g_assert(theme != NULL);
    if (G_UNLIKELY(!theme)) {
        g_warning(_("No icon theme!"));
        return NULL;
    }

    /* choose icons from directory best matching the requested size */
    sizedir = get_icon_sizedir(size);

    if (symbol_name == NULL || strlen(symbol_name) == 0)
        symbol_name = symbol_names[SYMBOL_NODATA];
    else if (night)
        suffix = "-night";

    /* check whether icon has been verified to be missing before */
    if (!icon_missing(theme, sizedir, symbol_name, suffix)) {
        filename = make_icon_filename(theme, sizedir, symbol_name, suffix);
        image = gdk_pixbuf_new_from_file_at_scale(filename, size, size, TRUE, NULL);
    }

    if (image == NULL) {
        /* remember failure for future lookups */
        if (filename) {
            weather_debug("Unable to open image: %s", filename);
            remember_missing_icon(theme, sizedir, symbol_name, suffix);
            g_free(filename);
            filename = NULL;
        }

        if (strcmp(symbol_name, symbol_names[SYMBOL_NODATA]))
            if (night)
                /* maybe there is no night icon, so fallback to using day icon... */
                return get_icon(theme, symbol_name, size, FALSE);
            else
                /* ... or use NODATA if we tried that already */
                return get_icon(theme, NULL, size, FALSE);
        else {
            /* last chance: get NODATA icon from standard theme */
            filename = make_fallback_icon_filename(sizedir);
            image = gdk_pixbuf_new_from_file_at_scale(filename, size, size,
                                                      TRUE, NULL);
            if (G_UNLIKELY(image == NULL))
                g_warning("Failed to open fallback icon from standard theme: %s",
                          filename);
        }
    }
    g_free(filename);

    return image;
}


/*
 * Create a new icon theme struct, initializing caches to undefined.
 */
static icon_theme *
make_icon_theme(void)
{
    icon_theme *theme = g_slice_new0(icon_theme);

    g_assert(theme != NULL);
    if (theme == NULL)
        return NULL;
    theme->missing_icons = g_array_new(FALSE, TRUE, sizeof(gchar *));
    return theme;
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

        if ((theme = make_icon_theme()) == NULL) {
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


/*
 * Compare two icon_theme structs using their path names, returning
 * the result as a qsort()-style comparison function (less than zero
 * for first arg is less than second arg, zero for equal, greater zero
 * if first arg is greater than second arg).
 */
static gint
icon_theme_compare(gconstpointer a,
                   gconstpointer b)
{
    icon_theme *it1 = *(icon_theme **) a;
    icon_theme *it2 = *(icon_theme **) b;

    if (G_UNLIKELY(it1 == NULL && it2 == NULL))
        return 0;
    if (G_UNLIKELY(it1 == NULL))
        return -1;
    if (G_UNLIKELY(it2 == NULL))
        return 1;

    return g_strcmp0(it1->dir, it2->dir);
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
        themes = g_array_new(FALSE, TRUE, sizeof(icon_theme *));

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
        weather_debug("Found %d icon theme(s) in %s.", themes->len, path);
    } else
        weather_debug("Could not list directory %s.", path);
    g_array_sort(themes, (GCompareFunc) icon_theme_compare);
    return themes;
}


/*
 * Returns the user icon theme directory as a string which needs to be
 * freed by the calling function.
 */
gchar *
get_user_icons_dir(void)
{
    return g_strconcat(g_get_user_config_dir(), G_DIR_SEPARATOR_S,
                       "xfce4", G_DIR_SEPARATOR_S, "weather",
                       G_DIR_SEPARATOR_S, "icons", NULL);
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
    dir = get_user_icons_dir();
    found = find_themes_in_dir(dir);
    g_free(dir);
    if (found) {
        if (found->len > 0)
            themes = g_array_append_vals(themes, found->data, found->len);
        g_array_free(found, FALSE);
    }

    /* next find themes in system directory */
    found = find_themes_in_dir(THEMESDIR);
    if (found) {
        if (found->len > 0)
            themes = g_array_append_vals(themes, found->data, found->len);
        g_array_free(found, FALSE);
    }

    weather_debug("Found %d icon themes in total.", themes->len, dir);
    return themes;
}


icon_theme *
icon_theme_copy(icon_theme *src)
{
    icon_theme *dst;

    if (G_UNLIKELY(src == NULL))
        return NULL;

    dst = make_icon_theme();
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
    gchar *missing;
    guint i;

    g_assert(theme != NULL);
    if (G_UNLIKELY(theme == NULL))
        return;
    g_free(theme->dir);
    g_free(theme->name);
    g_free(theme->author);
    g_free(theme->description);
    g_free(theme->license);
    for (i = 0; i < theme->missing_icons->len; i++) {
        missing = g_array_index(theme->missing_icons, gchar *, i);
        g_free(missing);
    }
    g_array_free(theme->missing_icons, FALSE);
    g_slice_free(icon_theme, theme);
}
