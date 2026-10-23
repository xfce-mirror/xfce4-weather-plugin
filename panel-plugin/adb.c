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
    cmd = malloc((argc + 2) * sizeof *cmd);
    cmd[0] = "adb";

    if (argc > 0) {
        memcpy(cmd + 1, argv, argc * sizeof *cmd);
    }

    cmd[argc + 1] = NULL;

    if (!g_spawn_sync(NULL, cmd, NULL, G_SPAWN_SEARCH_PATH,
                      NULL, NULL, &stdout_str, NULL, &status, &error)) {
        /* adb binary not found, or fork failed */
        weather_debug("loc: spawn failed: %s\n", error->message);
        g_clear_error(&error);
        free(cmd);
        return NULL;
    }

    if (!g_spawn_check_exit_status(status, &error)) {
        /* adb ran but exited non-zero */
        weather_debug("loc: adb exited %d: %s\n", status, error->message);
        g_clear_error(&error);
        g_free(stdout_str);
        free(cmd);
        return NULL;
    }

    weather_debug("loc: %zu bytes from adb",
                  stdout_str ? strlen(stdout_str) : 0);
    free(cmd);
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

    if (out == NULL) {
        return NULL;
    }

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
        }
    }

    /* free the ORIGINAL pointer */
    g_free(raw);
    g_match_info_free(mi);
    g_regex_unref(re);

    if (n_out)
        *n_out = line_num;

    return ds;
}


/* Runs `adb shell dumpsys location` */
gchar *
loc_dump(void) {
    gchar *command[] = {"shell", "dumpsys", "location", NULL};
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
    g_regex_unref(re);
    return ok;
}


/* Queries the phone and parses the first loc found. On
   success, allocates *lat_str and *lon_str (caller frees) and returns TRUE. Returns
   FALSE on any adb failure or unparseable output, leaving the output
   pointers untouched. */
gboolean
get_loc(gchar **lat_str, gchar **lon_str) {
    gchar *dump = loc_dump();
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


/* Refreshes data->lat/lon from an attached phone, then kicks off
   async lookups for the derived values: location name, altitude, and
   timezone. Optional convenience: the plugin continues to work
   normally without a phone, without the required binaries (like adb),
   or with the device unauthorized. Returns TRUE if the location was
   updated, FALSE otherwise (silent no-op). */
gboolean
update_loc(plugin_data *data) {
    gchar *new_lat = NULL;
    gchar *new_lon = NULL;
    gchar *old_lat, *old_lon;
    gdouble old_lat_d, old_lon_d, new_lat_d, new_lon_d;
    gdouble moved = 0.0;

    if (get_loc(&new_lat, &new_lon)) {
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

                return FALSE;
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

        return TRUE;
    }

    return FALSE;
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
 * ADB devices and shows or hides the "Update location from phone:" menu
 * item accordingly, so stale entries don't appear when no phone is
 * plugged in.
 *
 * Returns FALSE so the panel's own handler still runs and shows the menu.
 */
static gboolean cb_show_phones(GtkWidget *parent_menu, GdkEventButton *event, gpointer user_data) {
    weather_debug("loc: 'Update location from phone:' menu is activated");
    (void) parent_menu;
    plugin_data *data = (plugin_data *) user_data;

    if (event->button == 3 && data->update_loc_menu) {
        weather_debug("loc: Right-click detected before menu popup!");
        gsize devices;
        get_devices(&devices);
        gtk_widget_set_visible(data->update_loc_menu, devices > 0);
    }

    return FALSE;
}


const adb_t adb = {
    .cb_show_phones = cb_show_phones
};
