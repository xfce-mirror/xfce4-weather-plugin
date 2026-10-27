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

#include "adb.h"

/* runs a generic adb command. return stdout, or NULL on any
   failure: adb missing, no device attached, device unauthorized, or a
   non-zero exit. Caller frees the returned string.
 */
gchar *
call_adb(gchar **argv) {
    weather_debug("loc: spawning adb");
    GError *error = NULL;
    gchar *stdout_str = NULL;
    gint status;

    /* build command */
    size_t argc = 0;
    gchar **cmd;

    while (argv[argc]) argc++;
    cmd = g_new(gchar*, argc+2);
    cmd[0] = "adb";

    if (argc > 0) {
        memcpy(cmd + 1, argv, argc * sizeof *cmd);
    }

    cmd[argc + 1] = NULL;

    if (!g_spawn_sync(NULL, cmd, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, &stdout_str, NULL, &status, &error)) {
        /* adb binary not found, or fork failed */
        weather_debug("loc: spawn failed: %s", error->message);
        g_clear_error(&error);
        g_free(stdout_str);
        g_free(cmd);
        return NULL;
    }

    if (!g_spawn_check_exit_status(status, &error)) {
        /* adb ran but exited non-zero */
        weather_debug("loc: adb exited %d: %s", status, error->message);
        g_clear_error(&error);
        g_free(stdout_str);
        g_free(cmd);
        return NULL;
    }

    weather_debug("loc: %zu bytes from adb",
                  stdout_str ? strlen(stdout_str) : 0);
    g_free(cmd);
    return stdout_str;
}


/* Runs `adb devices`.
 * Returns list of devices, defined by serial, last element is NULL
 */
device *
get_devices(gsize *n_out) {
    gchar *command[] = {"devices", "-l", NULL};
    gchar *out = call_adb(command);
    /* keep original for free() */
    gchar *raw = out;
    gchar *line_start, *line_end;
    gsize n = 0, line_num = 0;
    device *ds;
    gchar *line;
    GMatchInfo *mi = NULL;
    static GRegex *re = NULL;

    if (n_out)
        *n_out = 0;
    if (out == NULL)
        return NULL;

    while (*out) {
        line_start = out;
        line_end = strchr(out, '\n');

        if (line_end != NULL) {
            *line_end = '\0';
            out = line_end + 1;
        } else {
            out = line_start + strlen(line_start);
        }

        if (!strncmp(line_start, "List of devices attached", 24)) {
            break;
        }
    }

    /* count non-empty lines remaining */
    for (gchar *c = out; *c; c++) {
        if (*c != '\n' && (c == out || c[-1] == '\n'))
            n++;
    }

    /* +1 for trailing NULL */
    ds = g_new0(device, n + 1);

    if (re == NULL) {
        /* Parse one line of `adb devices -l` output.
         *
         * Example line:
         *   51444d4852443498       device usb:3-1 product:starqltecs model:SM_G960W transport_id:2
         *
         * Groups:
         *   1 = serial   (may contain spaces and colons)
         *   2 = model    (value after "model:")
         *
         * Breakdown:
         *   ^(.+?)               Lazy: capture the shortest serial that lets the
         *                        rest of the pattern match. Lazy (not greedy) so
         *                        it doesn't swallow the state and properties.
         *   \\s+                 One or more spaces (adb pads columns to align).
         *   [^\\s:]+             The state token (e.g. "device", "offline",
         *                        "unauthorized"). Deliberately NOT a whitelist of
         *                        known states and NOT `\\S+`, because we require it
         *                        to be colon-free. That's what distinguishes the
         *                        state from the `key:value` properties that follow.
         *   \\s+                 Separator before the property block.
         *   (?:\\S+:\\S+\\s+)*   Zero or more `key:value` properties (usb:3-1,
         *                        product:..., device:..., transport_id:...).
         *                        Non-capturing. `*` (not `+`) so a line where
         *                        "model:" is the FIRST property still matches.
         *   \\bmodel:            Literal "model:", with a word boundary so we
         *                        don't match e.g. "devicemodel:" or "submodel:".
         *   (\\S+)               Model value: non-space run. `\\S+` (not `.+?\\s`)
         *                        so it works when the model is the LAST token on
         *                        the line (no trailing space to anchor on).
         */
        re = g_regex_new(
            "^(.+?)\\s+[^\\s:]+\\s+(?:\\S+:\\S+\\s+)*\\bmodel:(\\S+)",
            0, 0, NULL);
    }

    while ((line = strsep(&out, "\n")) != NULL) {
        if (g_regex_match(re, line, 0, &mi)) {
            ds[line_num].serial = g_match_info_fetch(mi, 1);
            ds[line_num].model = g_match_info_fetch(mi, 2);

            weather_debug("loc: serial=%s, model=%s", ds[line_num].serial,
                          ds[line_num].model);

            line_num++;
            g_match_info_free(mi);
            mi = NULL;
        }
    }

    /* free the ORIGINAL pointer */
    g_free(raw);

    if (n_out)
        *n_out = line_num;

    return ds;
}


/* Runs `adb -s @serial shell dumpsys location` */
gchar *
loc_dump(gchar *serial) {
    gchar *command[] = {"-s", serial, "shell", "dumpsys", "location", NULL};
    return call_adb(command);
}


/* Parses the first entry from adb dumpsys location dump. On success,
   allocates *lat_str and *lon_str (caller frees) and returns TRUE. Returns FALSE if
   the dump does not match, leaving the output pointers untouched. */
gboolean
parse_dump(const gchar *dump, gchar **lat_str, gchar **lon_str) {
    GMatchInfo *mi = NULL;
    gboolean ok = FALSE;
    static GRegex *re = NULL;

    /* first entry is the one with highest-confidence. */
    if (re == NULL)
        re = g_regex_new("Location\\[fused ([-0-9.]+),([-0-9.]+)", 0, 0, NULL);

    if (g_regex_match(re, dump, 0, &mi)) {
        *lat_str = g_match_info_fetch(mi, 1);
        *lon_str = g_match_info_fetch(mi, 2);
        weather_debug("loc: new lat=%s, lon=%s", *lat_str, *lon_str);
        ok = TRUE;
    } else {
        weather_debug("loc: no match");
    }

    g_match_info_free(mi);
    return ok;
}


/*
 * get_loc:
 * @serial:    serial of the ADB device to query. Must be non-NULL and
 *             NUL-terminated; the string is borrowed, not owned, and is
 *             passed straight through to `adb -s <serial>`.
 * @lat_str:   (out) (optional): on success, receives a newly-allocated
 *             string containing the latitude parsed from the dump. The
 *             caller owns it and must g_free() it. Left untouched on
 *             failure.
 * @lon_str:   (out) (optional): on success, receives a newly-allocated
 *             string containing the longitude parsed from the dump. The
 *             caller owns it and must g_free() it. Left untouched on
 *             failure.
 *
 * Convenience wrapper around loc_dump() + parse_dump(). Runs
 * `adb -s <serial> shell dumpsys location`, then extracts the first
 * `Location[fused lat,lon` entry from the output.
 *
 * Returns FALSE immediately if @serial is NULL, if loc_dump() returns
 * NULL (adb missing, no such device, unauthorized, non-zero exit), or
 * if parse_dump() finds no match. In every failure case @lat_str and
 * @lon_str are left untouched, so the caller can distinguish "not
 * updated" from "updated to empty" by initializing them to NULL and
 * checking the return value.
 *
 * On success both @lat_str and @lon_str are set to freshly-allocated
 * strings and TRUE is returned.
 */
gboolean
get_loc(gchar *serial, gchar **lat_str, gchar **lon_str) {
    if (serial == NULL)
        return FALSE;

    gchar *dump = loc_dump(serial);
    gboolean ok;

    if (dump == NULL)
        return FALSE;

    ok = parse_dump(dump, lat_str, lon_str);

    g_free(dump);
    return ok;
}


/* great-circle distance between two points in kilometers (spherical) */
gdouble
distance_km(gdouble x_lat, gdouble x_lon, gdouble y_lat, gdouble y_lon) {
    gdouble dlat, dlon, a;

    dlat = (y_lat - x_lat) * DEG_TO_RAD;
    dlon = (y_lon - x_lon) * DEG_TO_RAD;

    a = sin(dlat / 2) * sin(dlat / 2)
        + cos(x_lat * DEG_TO_RAD) * cos(y_lat * DEG_TO_RAD)
        * sin(dlon / 2) * sin(dlon / 2);

    return EARTH_RADIUS_KM * 2 * atan2(sqrt(a), sqrt(1.0 - a));
}


/*
 * update_loc:
 * @item:      the GtkMenuItem that was activated. For the single-device
 *             case this is data->update_loc_widget itself; for the
 *             multi-device case it is one of the per-device submenu
 *             items. Either way, its "serial" user data identifies the
 *             phone to query.
 * @user_data: plugin_data for this plugin instance
 *
 * "Update location from phone" activation handler. Queries the phone
 * named by the "serial" user data on @item via get_loc(), and, if a
 * position is returned:
 *
 *   - Converts the old and new coordinates to doubles and computes the
 *     great-circle displacement. If it is below MIN_DISPLACEMENT the
 *     update is skipped entirely (the fresh strings are freed and the
 *     function returns early), so GPS jitter does not cause needless
 *     refetches.
 *   - Otherwise replaces data->lat / data->lon with the new strings
 *     (freeing the old ones) and fires the derived lookups:
 *       * reverse_geocode()      always — the name is what the user sees
 *       * lookup_altitude()      if moved >= ALTITUDE_DISPLACEMENT
 *       * lookup_timezone()      if moved >= TIMEZONE_DISPLACEMENT
 *     and then calls update_weatherdata_with_reset().
 *
 * These lookups are asynchronous; this function does not wait for them.
 *
 * "serial" is borrowed, not owned: it is read from @item and never
 * freed here. If get_loc() fails (adb missing, no device, unauthorized,
 * unparseable dump) the function does nothing and leaves
 * data->lat / data->lon untouched.
 *
 * Returns: void (GTK activate handler signature).
 */
void
update_loc(GtkMenuItem *item, gpointer user_data) {
    plugin_data *data = user_data;
    gchar *new_lat = NULL;
    gchar *new_lon = NULL;
    gchar *old_lat, *old_lon;
    gdouble old_lat_d, old_lon_d, new_lat_d, new_lon_d;
    gdouble moved = 0.0;
    gchar *serial = g_object_get_data(G_OBJECT(item), "serial");
    weather_debug("loc: getting loc from %s", serial);

    if (get_loc(serial, &new_lat, &new_lon)) {
        old_lat = data->lat;
        old_lon = data->lon;
        weather_debug("loc: old lat=%s, lon=%s", old_lat, old_lon);

        /* skip the update if the phone is roughly where it already was: */
        if (old_lat != NULL && old_lon != NULL) {
            old_lat_d = g_ascii_strtod(old_lat, NULL);
            old_lon_d = g_ascii_strtod(old_lon, NULL);
            new_lat_d = g_ascii_strtod(new_lat, NULL);
            new_lon_d = g_ascii_strtod(new_lon, NULL);
            moved = distance_km(old_lat_d, old_lon_d, new_lat_d, new_lon_d);

            if (moved < MIN_DISPLACEMENT) {
                g_free(new_lat);
                g_free(new_lon);

                weather_debug("loc: too close");
                return;
            }
        }

        g_free(data->lat);
        g_free(data->lon);

        data->lat = new_lat;
        data->lon = new_lon;

        /* three async lookups fire here; the name is what the user
           sees so it always refetches, while altitude and timezone
           only change meaningfully over longer distances. */
        reverse_geocode(data, data->lat, data->lon);

        if (moved >= ALTITUDE_DISPLACEMENT)
            lookup_altitude(data, data->lat, data->lon);

        if (moved >= TIMEZONE_DISPLACEMENT)
            lookup_timezone(data, data->lat, data->lon);

        update_weatherdata_with_reset(data);
    }
}

void free_devices(device *ds) {
    if (!ds)
        return;
    for (device *p = ds; p->model != NULL; p++) {
        g_free(p->serial);
        g_free(p->model);
    }
    g_free(ds);
}

/*
 * cb_show_phones:
 * @parent_menu: the panel's GtkMenu (emitted by button-press-event)
 * @event:       the button press event; only right-clicks (button 3)
 *               are of interest
 * @user_data:   plugin_data for this plugin instance
 *
 * Runs *before* the panel pops up its right-click menu (see the handler
 * ordering note in xfceweather_create_control()). Queries the connected
 * ADB devices via `adb devices -l` and reshapes
 * data->update_loc_widget to match the current device count:
 *
 *   - 0 devices: the menu item is hidden and any top-level "activate"
 *     handler is disconnected, so a stale entry never appears when no
 *     phone is plugged in.
 *   - 1 device:  the item is relabeled "Update location from <model>",
 *     any submenu is removed, and a single "activate" handler is
 *     connected (exactly once, guarded by the "activate-connected"
 *     user-data flag). The device serial is stored on the item so
 *     update_loc() knows which phone to query.
 *   - >1 device: the item gets a submenu with one entry per device,
 *     each labeled with its model and carrying its own serial. The
 *     top-level "activate" handler and serial are removed so the
 *     parent item cannot fire update_loc() with a stale serial.
 *
 * The item is shown only when at least one device is attached.
 *
 * Returns FALSE so the panel's own handler still runs and shows the menu.
 */
static gboolean cb_show_phones(GtkWidget *parent_menu, GdkEventButton *event,
                               gpointer user_data) {
    weather_debug("loc: 'Update location from phone:' menu is activated");
    (void) parent_menu;
    plugin_data *data = user_data;
    gsize num_devices = 0;
    device *ds;
    GtkWidget *submenu, *sub_item;
    GtkMenuItem *update_loc_menu_item = GTK_MENU_ITEM(data->update_loc_widget);
    gchar *new_label;

    if (event->button == 3 && data->update_loc_widget) {
        weather_debug("loc: Right-click detected before menu popup!");
        ds = get_devices(&num_devices);
        weather_debug("loc: %zu device(s) found", num_devices);

        if (num_devices > 1) {
            gtk_menu_item_set_label(update_loc_menu_item,
                                    _("Update location from phone:"));
            submenu = gtk_menu_new();

            for (gsize i = 0; i < num_devices; i++) {
                device d = ds[i];
                weather_debug("loc: model='%s', serial='%s'", d.model,
                              d.serial);

                sub_item = gtk_menu_item_new_with_label(d.model);

                // associate serial with menu item
                g_object_set_data_full(G_OBJECT(sub_item), "serial",
                                       g_strdup(d.serial), g_free);
                g_signal_connect(sub_item, "activate",
                                 G_CALLBACK(update_loc), data);

                gtk_menu_shell_append(GTK_MENU_SHELL(submenu), sub_item);
            }

            gtk_menu_item_set_submenu(update_loc_menu_item, submenu);
            gtk_widget_show_all(submenu);
        } else if (num_devices == 1) {
            gtk_menu_item_set_submenu(update_loc_menu_item, NULL);
            new_label =
                    g_strdup_printf(_("Update location from %s"), ds->model);
            gtk_menu_item_set_label(update_loc_menu_item, new_label);
            g_free(new_label);
            // associate serial with menu item
            g_object_set_data_full(G_OBJECT(update_loc_menu_item), "serial",
                                   g_strdup(ds[0].serial), g_free);

            /* (re)connect signal exactly once */
            if (!g_object_get_data(
                G_OBJECT(update_loc_menu_item), "activate-connected")) {
                g_signal_connect(update_loc_menu_item, "activate",
                                 G_CALLBACK(update_loc), data);
                g_object_set_data(G_OBJECT(update_loc_menu_item),
                                  "activate-connected", GINT_TO_POINTER(1));
            }
        } else {
            g_signal_handlers_disconnect_by_func(update_loc_menu_item,
                                                 G_CALLBACK(update_loc), data);
            g_object_set_data(G_OBJECT(update_loc_menu_item),
                              "activate-connected", NULL);
            gtk_menu_item_set_submenu(update_loc_menu_item, NULL);
        }

        free_devices(ds);
        gtk_widget_set_visible(data->update_loc_widget, num_devices > 0);
    }

    return FALSE;
}


const adb_t adb = {
    .cb_show_phones = cb_show_phones
};
