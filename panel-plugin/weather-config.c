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

#include <string.h>
#include <libxfce4ui/libxfce4ui.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"
#include "weather-debug.h"
#include "weather-config.h"
#include "weather-search.h"
#include "weather-scrollbox.h"

#define UPDATE_TIMER_DELAY 7
#define OPTIONS_N 15
#define BORDER 4
#define LOC_NAME_MAX_LEN 50
#define TIMEZONE_MAX_LEN 40

#define ADD_PAGE(homogenous)                                        \
    palign = gtk_alignment_new(0.5, 0.5, 1, 1);                     \
    gtk_container_set_border_width(GTK_CONTAINER(palign), BORDER);  \
    page = gtk_vbox_new(homogenous, BORDER);                        \
    gtk_container_add(GTK_CONTAINER(palign), page);

#define ADD_LABEL(text, sg)                                         \
    label = gtk_label_new_with_mnemonic(text);                      \
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);                \
    if (G_LIKELY(sg))                                               \
        gtk_size_group_add_widget(sg, label);                       \
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, BORDER);

#define ADD_SPIN(spin, min, max, step, val, digits, sg)                 \
    spin = gtk_spin_button_new_with_range(min, max, step);              \
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), val);              \
    gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin), digits);          \
    if (G_LIKELY(sg))                                                   \
        gtk_size_group_add_widget(sg, spin);                            \
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(spin));  \
    gtk_box_pack_start(GTK_BOX(hbox), spin, FALSE, FALSE, 0);

#define ADD_COMBO(combo)                                                \
    combo = gtk_combo_box_new_text();                                   \
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(combo)); \
    gtk_box_pack_start(GTK_BOX(hbox), combo, TRUE, TRUE, 0);

#define ADD_COMBO_VALUE(combo, text)                        \
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), text);

#define SET_COMBO_VALUE(combo, val)                         \
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), val);

#define ADD_LABEL_EDIT_BUTTON(button, text, icon, cb_func)          \
    button = gtk_button_new_with_mnemonic(text);                    \
    image = gtk_image_new_from_stock(icon, GTK_ICON_SIZE_BUTTON);   \
    gtk_button_set_image(GTK_BUTTON(button), image);                \
    gtk_size_group_add_widget(sg_button, button);                   \
    g_signal_connect(G_OBJECT(button), "clicked",                   \
                     G_CALLBACK(cb_func), dialog);

#define SET_TOOLTIP(widget, markup)                             \
    gtk_widget_set_tooltip_markup(GTK_WIDGET(widget), markup);

#define TEXT_UNKNOWN(text) (text ? text : "-")

static const labeloption labeloptions[OPTIONS_N] = {
    /*
     * TRANSLATORS: The abbreviations in parentheses will be shown in
     * the scrollbox together with the values. Keep them in sync with
     * those in make_label() in weather.c. Some of them may be
     * standardized internationally, like CL, CM, CH, and you might
     * read that up somewhere and decide whether you want to use them
     * or not. In general, though, you should just try to choose
     * letter(s) that make sense and don't use up too much space.
     */
    {N_("Temperature (T)"), TEMPERATURE},
    {N_("Barometric pressure (P)"), PRESSURE},
    {N_("Wind speed (WS)"), WIND_SPEED},
    {N_("Wind speed - Beaufort scale (WB)"), WIND_BEAUFORT},
    {N_("Wind direction (WD)"), WIND_DIRECTION},
    {N_("Wind direction in degrees (WD)"), WIND_DIRECTION_DEG},
    {N_("Humidity (H)"), HUMIDITY},
    {N_("Dew point (D)"), DEWPOINT},
    {N_("Apparent temperature (A)"), APPARENT_TEMPERATURE},
    {N_("Low clouds (CL)"), CLOUDS_LOW},
    {N_("Middle clouds (CM)"), CLOUDS_MID},
    {N_("High clouds (CH)"), CLOUDS_HIGH},
    {N_("Cloudiness (C)"), CLOUDINESS},
    {N_("Fog (F)"), FOG},
    {N_("Precipitation (R)"), PRECIPITATION},
};

static void
spin_alt_value_changed(const GtkWidget *spin,
                       gpointer user_data);


static void
setup_units(xfceweather_dialog *dialog,
            const units_config *units);


static void
update_summary_window(xfceweather_dialog *dialog,
                      gboolean restore_position)
{
    if (dialog->pd->summary_window) {
        gint x, y;

        /* remember position */
        if (restore_position)
            gtk_window_get_position(GTK_WINDOW(dialog->pd->summary_window),
                                    &x, &y);

        /* call toggle function two times to close and open dialog */
        forecast_click(dialog->pd->summary_window, dialog->pd);
        forecast_click(dialog->pd->summary_window, dialog->pd);

        /* ask wm to restore position of new window */
        if (restore_position)
            gtk_window_move(GTK_WINDOW(dialog->pd->summary_window), x, y);

        /* bring config dialog to the front, it might have been hidden
         * beneath the summary window */
        gtk_window_present(GTK_WINDOW(dialog->dialog));
    }
}


static gboolean
schedule_data_update(gpointer user_data)
{
    xfceweather_dialog *dialog = user_data;
    plugin_data *pd = dialog->pd;

    /* force update of downloaded data */
    weather_debug("Delayed update timer expired, now scheduling data update.");
    update_weatherdata_with_reset(pd);

    if (dialog->update_spinner && GTK_IS_SPINNER(dialog->update_spinner)) {
        gtk_spinner_stop(GTK_SPINNER(dialog->update_spinner));
        gtk_widget_hide(GTK_WIDGET(dialog->update_spinner));
    }
    return FALSE;
}


static void
schedule_delayed_data_update(xfceweather_dialog *dialog)
{
    GSource *source;

    weather_debug("Starting delayed data update.");
    /* cancel any update that was scheduled before */
    if (dialog->timer_id) {
        source = g_main_context_find_source_by_id(NULL, dialog->timer_id);
        if (source) {
            g_source_destroy(source);
            dialog->timer_id = 0;
        }
    }

    /* stop any updates that could be performed by weather.c */
    if (dialog->pd->update_timer) {
        source = g_main_context_find_source_by_id(NULL, dialog->pd->update_timer);
        if (source) {
            g_source_destroy(source);
            dialog->pd->update_timer = 0;
        }
    }

    gtk_widget_show(GTK_WIDGET(dialog->update_spinner));
    gtk_spinner_start(GTK_SPINNER(dialog->update_spinner));
    dialog->timer_id =
        g_timeout_add_seconds(UPDATE_TIMER_DELAY, schedule_data_update, dialog);
}


static gchar *
sanitize_location_name(const gchar *location_name)
{
    gchar *pos, *pos2, sane[LOC_NAME_MAX_LEN * 4];
    glong len, offset;

    pos = g_utf8_strchr(location_name, -1, ',');
    if (pos != NULL) {
        pos2 = pos;
        while ((pos2 = g_utf8_next_char(pos2)))
            if (g_utf8_get_char(pos2) == ',') {
                pos = pos2;
                break;
            }
        offset = g_utf8_pointer_to_offset(location_name, pos);
        if (offset > LOC_NAME_MAX_LEN)
            offset = LOC_NAME_MAX_LEN;
        (void) g_utf8_strncpy(sane, location_name, offset);
        sane[LOC_NAME_MAX_LEN * 4 - 1] = '\0';
        return g_strdup(sane);
    } else {
        len = g_utf8_strlen(location_name, LOC_NAME_MAX_LEN);

        if (len >= LOC_NAME_MAX_LEN) {
            (void) g_utf8_strncpy(sane, location_name, len);
            sane[LOC_NAME_MAX_LEN * 4 - 1] = '\0';
            return g_strdup(sane);
        }

        if (len > 0)
            return g_strdup(location_name);

        return g_strdup(_("Unset"));
    }
}


static void
cb_lookup_altitude(SoupSession *session,
                   SoupMessage *msg,
                   gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    xml_altitude *altitude;
    gdouble alt;

    altitude = (xml_altitude *)
        parse_xml_document(msg, (XmlParseFunc) parse_altitude);

    if (altitude) {
        alt = string_to_double(altitude->altitude, -9999);
        xml_altitude_free(altitude);
    }
    weather_debug("Altitude returned by GeoNames: %.0f meters", alt);
    if (alt < -420)
        alt = 0;
    else if (dialog->pd->units->altitude == FEET)
        alt /= 0.3048;
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->spin_alt), alt);
}


static void
cb_lookup_timezone(SoupSession *session,
                   SoupMessage *msg,
                   gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    xml_timezone *timezone;

    timezone = (xml_timezone *)
        parse_xml_document(msg, (XmlParseFunc) parse_timezone);
    weather_dump(weather_dump_timezone, timezone);

    if (timezone) {
        gtk_entry_set_text(GTK_ENTRY(dialog->text_timezone),
                           timezone->timezone_id);
        xml_timezone_free(timezone);
    } else
        gtk_entry_set_text(GTK_ENTRY(dialog->text_timezone), "");
}


static void
lookup_altitude_timezone(const gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    gchar *url, *latstr, *lonstr;
    gdouble lat, lon;

    lat = gtk_spin_button_get_value(GTK_SPIN_BUTTON(dialog->spin_lat));
    lon = gtk_spin_button_get_value(GTK_SPIN_BUTTON(dialog->spin_lon));

    latstr = double_to_string(lat, "%.6f");
    lonstr = double_to_string(lon, "%.6f");

    /* lookup altitude */
    url = g_strdup_printf("https://secure.geonames.org"
                          "/srtm3XML?lat=%s&lng=%s&username=%s",
                          latstr, lonstr,
                          dialog->pd->geonames_username
                          ? dialog->pd->geonames_username : GEONAMES_USERNAME);
    weather_http_queue_request(dialog->pd->session, url,
                               cb_lookup_altitude, user_data);
    g_free(url);

    /* lookup timezone */
    url = g_strdup_printf("https://secure.geonames.org"
                          "/timezone?lat=%s&lng=%s&username=%s",
                          latstr, lonstr,
                          dialog->pd->geonames_username
                          ? dialog->pd->geonames_username : GEONAMES_USERNAME);
    weather_http_queue_request(dialog->pd->session, url,
                               cb_lookup_timezone, user_data);
    g_free(url);

    g_free(lonstr);
    g_free(latstr);
}


static void
auto_locate_cb(const gchar *loc_name,
               const gchar *lat,
               const gchar *lon,
               const units_config *units,
               gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;

    if (lat && lon && loc_name) {
        gtk_entry_set_text(GTK_ENTRY(dialog->text_loc_name), loc_name);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->spin_lat),
                                  string_to_double(lat, 0));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->spin_lon),
                                  string_to_double(lon, 0));
        lookup_altitude_timezone(user_data);
    } else
        gtk_entry_set_text(GTK_ENTRY(dialog->text_loc_name), _("Unset"));
    setup_units(dialog, units);
    gtk_widget_set_sensitive(dialog->text_loc_name, TRUE);
}


static void
start_auto_locate(xfceweather_dialog *dialog)
{
    gtk_widget_set_sensitive(dialog->text_loc_name, FALSE);
    gtk_entry_set_text(GTK_ENTRY(dialog->text_loc_name), _("Detecting..."));
    gtk_spinner_start(GTK_SPINNER(dialog->update_spinner));
    weather_search_by_ip(dialog->pd->session, auto_locate_cb, dialog);
}


static gboolean
cb_findlocation(GtkButton *button,
                gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    search_dialog *sdialog;
    gchar *loc_name, *lat, *lon;

    sdialog = create_search_dialog(NULL, dialog->pd->session);

    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    if (run_search_dialog(sdialog)) {
        /* limit digit precision of coordinates from search results */
        lat = double_to_string(string_to_double(sdialog->result_lat, 0),
                               "%.6f");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->spin_lat),
                                  string_to_double(lat, 0));
        lon = double_to_string(string_to_double(sdialog->result_lon, 0),
                               "%.6f");
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->spin_lon),
                                  string_to_double(lon, 0));
        loc_name = sanitize_location_name(sdialog->result_name);
        gtk_entry_set_text(GTK_ENTRY(dialog->text_loc_name), loc_name);
        g_free(loc_name);
        g_free(lon);
        g_free(lat);
    }
    free_search_dialog(sdialog);

    lookup_altitude_timezone(user_data);
    gtk_widget_set_sensitive(GTK_WIDGET(button), TRUE);

    return FALSE;
}


static void
setup_altitude(xfceweather_dialog *dialog)
{
    g_signal_handlers_block_by_func(dialog->spin_alt,
                                    G_CALLBACK(spin_alt_value_changed),
                                    dialog);
    switch (dialog->pd->units->altitude) {
    case FEET:
        gtk_label_set_text(GTK_LABEL(dialog->label_alt_unit),
                           _("feet"));
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(dialog->spin_alt),
                                  -1378.0, 32808);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->spin_alt),
                                  (gdouble) dialog->pd->msl / 0.3048);
        break;

    case METERS:
    default:
        gtk_label_set_text(GTK_LABEL(dialog->label_alt_unit),
                           _("meters"));
        gtk_spin_button_set_range(GTK_SPIN_BUTTON(dialog->spin_alt),
                                  -420, 10000);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(dialog->spin_alt),
                                  (gdouble) (dialog->pd->msl));
        break;
    }
    g_signal_handlers_unblock_by_func(dialog->spin_alt,
                                      G_CALLBACK(spin_alt_value_changed),
                                      dialog);
    return;
}


static void
text_loc_name_changed(const GtkWidget *spin,
                      gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    g_free(dialog->pd->location_name);
    dialog->pd->location_name =
        g_strdup(gtk_entry_get_text(GTK_ENTRY(dialog->text_loc_name)));
}


static void
spin_lat_value_changed(const GtkWidget *spin,
                       gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    gdouble val;

    val = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
    g_free(dialog->pd->lat);
    dialog->pd->lat = double_to_string(val, "%.6f");
    schedule_delayed_data_update(dialog);
}


static void
spin_lon_value_changed(const GtkWidget *spin,
                       gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    gdouble val;

    val = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
    g_free(dialog->pd->lon);
    dialog->pd->lon = double_to_string(val, "%.6f");
    schedule_delayed_data_update(dialog);
}


static void
spin_alt_value_changed(const GtkWidget *spin,
                       gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    gdouble val;

    val = gtk_spin_button_get_value(GTK_SPIN_BUTTON(spin));
    if (dialog->pd->units->altitude == FEET)
        val *= 0.3048;
    dialog->pd->msl = (gint) val;
    schedule_delayed_data_update(dialog);
}


static void
text_timezone_changed(const GtkWidget *entry,
                      gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;

    if (dialog->pd->timezone)
        g_free(dialog->pd->timezone);
    dialog->pd->timezone = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
    schedule_delayed_data_update(dialog);
}


static void
create_location_page(xfceweather_dialog *dialog)
{
    GtkWidget *button_loc_change;

    /* location name */
    dialog->text_loc_name = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "text_loc_name"));
    gtk_entry_set_max_length(GTK_ENTRY(dialog->text_loc_name),
                             LOC_NAME_MAX_LEN);

    button_loc_change = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "button_loc_change"));
    g_signal_connect(G_OBJECT(button_loc_change), "clicked",
                     G_CALLBACK(cb_findlocation), dialog);

    if (dialog->pd->location_name)
        gtk_entry_set_text(GTK_ENTRY(dialog->text_loc_name),
                           dialog->pd->location_name);
    else
        gtk_entry_set_text(GTK_ENTRY(dialog->text_loc_name), _("Unset"));

    /* update spinner */
    dialog->update_spinner = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "update_spinner"));

    /* latitude */
    dialog->spin_lat = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "spin_lat"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->spin_lat), string_to_double(dialog->pd->lat, 0));

    /* longitude */
    dialog->spin_lon = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "spin_lon"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->spin_lon), string_to_double(dialog->pd->lon, 0));

    /* altitude */
    dialog->label_alt_unit = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "label_alt_unit"));
    dialog->spin_alt = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "spin_alt"));
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->spin_alt), dialog->pd->msl);

    /* timezone */
    dialog->text_timezone = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "text_timezone"));
    gtk_entry_set_max_length(GTK_ENTRY(dialog->text_timezone),
                             LOC_NAME_MAX_LEN);
    if (dialog->pd->timezone)
        gtk_entry_set_text(GTK_ENTRY(dialog->text_timezone),
                           dialog->pd->timezone);
    else
        gtk_entry_set_text(GTK_ENTRY(dialog->text_timezone), "");

    /* set up the altitude spin box and unit label (meters/feet) */
    setup_altitude(dialog);
}


static void
combo_unit_temperature_set_tooltip(GtkWidget *combo)
{
    gchar *text = NULL;
    gint value = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

    switch (value) {
    case CELSIUS:
        text = _("Named after the astronomer Anders Celsius who invented the "
                 "original scale in 1742, the Celsius scale is an "
                 "international standard unit and nowadays defined "
                 "using the Kelvin scale. 0 °C is equivalent to 273.15 K "
                 "and 1 °C difference in temperature is exactly the same "
                 "difference as 1 K. It is defined with the melting point "
                 "of water being roughly at 0 °C and its boiling point at "
                 "100 °C at one standard atmosphere (1 atm = 1013.5 hPa). "
                 "Until 1948, the unit was known as <i>centigrade</i> - from "
                 "Latin <i>centum</i> (100) and <i>gradus</i> (steps).\n"
                 "In meteorology and everyday life the Celsius scale is "
                 "very convenient for expressing temperatures because its "
                 "numbers can be an easy indicator for the formation of "
                 "black ice and snow.");
        break;
    case FAHRENHEIT:
        text = _("The current Fahrenheit temperature scale is based on one "
                 "proposed in 1724 by the physicist Daniel Gabriel "
                 "Fahrenheit. 0 °F was the freezing point of brine on the "
                 "original scale at standard atmospheric pressure, which "
                 "was the lowest temperature achievable with this mixture "
                 "of ice, salt and ammonium chloride. The melting point of "
                 "water is at 32 °F and its boiling point at 212 °F. "
                 "The Fahrenheit and Celsius scales intersect at -40 "
                 "degrees. Even in cold winters, the temperatures usually "
                 "do not fall into negative ranges on the Fahrenheit scale.\n"
                 "With its inventor being a member of the Royal Society in "
                 "London and having a high reputation, the Fahrenheit scale "
                 "enjoyed great popularity in many English-speaking countries, "
                 "but was replaced by the Celsius scale in most of these "
                 "countries during the metrification process in the mid to "
                 "late 20th century.");
        break;
    }
    gtk_widget_set_tooltip_markup(GTK_WIDGET(combo), text);
}


static void
combo_unit_temperature_changed(GtkWidget *combo,
                               gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->units->temperature =
        gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    combo_unit_temperature_set_tooltip(combo);
    update_scrollbox(dialog->pd, TRUE);
    update_summary_window(dialog, TRUE);
}


static void
combo_unit_pressure_set_tooltip(GtkWidget *combo)
{
    gchar *text = NULL;
    gint value = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

    switch (value) {
    case HECTOPASCAL:
        text = _("The pascal, named after mathematician, physicist and "
                 "philosopher Blaise Pascal, is a SI derived unit and a "
                 "measure of force per unit area, defined as one newton "
                 "per square meter. One standard atmosphere (atm) is "
                 "1013.25 hPa.");
        break;
    case INCH_MERCURY:
        text = _("Inches of mercury is still widely used for barometric "
                 "pressure in weather reports, refrigeration and aviation "
                 "in the United States, but seldom used elsewhere. "
                 "It is defined as the pressure exerted by a 1 inch "
                 "circular column of mercury of 1 inch in height at "
                 "32 °F (0 °C) at the standard acceleration of gravity.");
        break;
    case PSI:
        text = _("The pound per square inch is a unit of pressure based on "
                 "avoirdupois units (a system of weights based on a pound of "
                 "16 ounces) and defined as the pressure resulting from a "
                 "force of one pound-force applied to an area of one square "
                 "inch. It is used in the United States and to varying "
                 "degrees in everyday life in Canada, the United Kingdom and "
                 "maybe some former British Colonies.");
        break;
    case TORR:
        text = _("The torr unit was named after the physicist and "
                 "mathematician Evangelista Torricelli who discovered the "
                 "principle of the barometer in 1644 and demonstrated the "
                 "first mercury barometer to the general public. A pressure "
                 "of 1 torr is approximately equal to one millimeter of "
                 "mercury, and one standard atmosphere (atm) equals 760 "
                 "Torr.");
        break;
    }
    gtk_widget_set_tooltip_markup(GTK_WIDGET(combo), text);
}


static void
combo_unit_pressure_changed(GtkWidget *combo,
                            gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->units->pressure =
        gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    combo_unit_pressure_set_tooltip(combo);
    update_scrollbox(dialog->pd, TRUE);
    update_summary_window(dialog, TRUE);
}


static void
combo_unit_windspeed_set_tooltip(GtkWidget *combo)
{
    gchar *text = NULL;
    gint value = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

    switch (value) {
    case KMH:
        text = _("Wind speeds in TV or in the news are often provided in "
                 "km/h.");
        break;
    case MPH:
        text = _("Miles per hour is an imperial unit of speed expressing the "
                 "number of statute miles covered in one hour.");
        break;
    case MPS:
        text = _("Meter per second is <i>the</i> unit typically used by "
                 "meteorologists to denote wind speeds.");
        break;
    case FTS:
        text = _("The foot per second (pl. feet per second) in the imperial "
                 "unit system is the counterpart to the meter per second in "
                 "the International System of Units.");
        break;
    case KNOTS:
        text = _("The knot is a unit of speed equal to one international "
                 "nautical mile (1.852 km) per hour, or approximately "
                 "1.151 mph, and sees worldwide use in meteorology and "
                 "in maritime and air navigation. A vessel travelling at "
                 "1 knot along a meridian travels one minute of geographic "
                 "latitude in one hour.");
        break;
    }
    gtk_widget_set_tooltip_markup(GTK_WIDGET(combo), text);
}


static void
combo_unit_windspeed_changed(GtkWidget *combo,
                             gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->units->windspeed =
        gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    combo_unit_windspeed_set_tooltip(combo);
    update_scrollbox(dialog->pd, TRUE);
    update_summary_window(dialog, TRUE);
}


static void
combo_unit_precipitation_set_tooltip(GtkWidget *combo)
{
    gchar *text = NULL;
    gint value = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

    switch (value) {
    case MILLIMETERS:
        text = _("1 millimeter is one thousandth of a meter - the fundamental "
                 "unit of length in the International System of Units -, or "
                 "approximately 0.04 inches.");
        break;
    case INCHES:
        text = _("The English word <i>inch</i> comes from Latin <i>uncia</i> "
                 "meaning <i>one-twelfth part</i> (in this case, one twelfth "
                 "of a foot). In the past, there have been many different "
                 "standards of the inch with varying sizes of measure, but "
                 "the current internationally accepted value is exactly 25.4 "
                 "millimeters.");
        break;
    }
    gtk_widget_set_tooltip_markup(GTK_WIDGET(combo), text);
}


static void
combo_unit_precipitation_changed(GtkWidget *combo,
                                 gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->units->precipitation =
        gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    combo_unit_precipitation_set_tooltip(combo);
    update_scrollbox(dialog->pd, TRUE);
    update_summary_window(dialog, TRUE);
}


static void
combo_unit_altitude_set_tooltip(GtkWidget *combo)
{
    gchar *text = NULL;
    gint value = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

    switch (value) {
    case METERS:
        text = _("The meter is the fundamental unit of length in the "
                 "International System of Units. Originally intended "
                 "to be one ten-millionth of the distance from the Earth's "
                 "equator to the North Pole at sea level, its definition "
                 "has been periodically refined to reflect growing "
                 "knowledge of metrology (the science of measurement).");
        break;
    case FEET:
        text = _("A foot (plural feet) is a unit of length defined as being "
                 "0.3048 m exactly and used in the imperial system of units "
                 "and United States customary units. It is subdivided into "
                 "12 inches. The measurement of altitude in the aviation "
                 "industry is one of the few areas where the foot is widely "
                 "used outside the English-speaking world.");
        break;
    }
    gtk_widget_set_tooltip_markup(GTK_WIDGET(combo), text);
}


static void
combo_unit_altitude_changed(GtkWidget *combo,
                            gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;

    dialog->pd->units->altitude =
        gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    combo_unit_altitude_set_tooltip(combo);
    setup_altitude(dialog);
    update_summary_window(dialog, TRUE);
}


static void
combo_apparent_temperature_set_tooltip(GtkWidget *combo)
{
    gchar *text = NULL;
    gint value = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

    switch (value) {
    case WINDCHILL_HEATINDEX:
        /*
         * TRANSLATORS: The Summer Simmer Index is similar to the heat
         * index, but usually used at night because of its better accuracy
         * at that time.
         */
        text = _("Used in North America, wind chill will be reported for low "
                 "temperatures and heat index for higher ones. At night, heat "
                 "index will be replaced by the Summer Simmer Index. For wind "
                 "chill, wind speeds need to be above 3.0 mph (4.828 km/h) "
                 "and air temperature below 50.0 °F (10.0 °C). For heat "
                 "index, air temperature needs to be above 80 °F (26.7 °C) "
                 "- or above 71.6 °F (22 °C) at night - and relative humidity "
                 "at least 40%. If these conditions are not met, the air "
                 "temperature will be shown.") ;
        break;
    case WINDCHILL_HUMIDEX:
        text = _("The Canadian counterpart to the US windchill/heat index, "
                 "with the wind chill being similar to the previous model "
                 "but with slightly different constraints. Instead of the "
                 "heat index <i>humidex</i> will be used. For wind chill "
                 "to become effective, wind speeds need to be above 2.0 "
                 "km/h (1.24 mph) and air temperature below or equal to "
                 "0 °C (32 °F). For humidex, air temperature needs to "
                 "be at least 20.0 °C (68 °F), with a dewpoint greater than "
                 "0 °C (32 °F). If these conditions are not met, the air "
                 "temperature will be shown.");
        break;
    case STEADMAN:
        text = _("This is the model used by the Australian Bureau of "
                 "Meteorology, especially adapted for the climate of this "
                 "continent. Possibly used in Central Europe and parts of "
                 "other continents too, but then windchill and similar values "
                 "had never gained that much popularity there as in the US "
                 "or Canada, so information about its usage is scarce or "
                 "uncertain. It depends on air temperature, wind speed and "
                 "humidity and can be used for lower and higher "
                 "temperatures alike.");
        break;
    case QUAYLE_STEADMAN:
        text = _("Improvements by Robert G. Quayle and Robert G. Steadman "
                 "applied in 1998 to earlier experiments/developments by "
                 "Steadman. This model only depends on wind speed and "
                 "temperature, not on relative humidity and can be used "
                 "for both heat and cold stress.");
        break;
    }
    gtk_widget_set_tooltip_markup(GTK_WIDGET(combo), text);
}


static void
combo_apparent_temperature_changed(GtkWidget *combo,
                                   gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->units->apparent_temperature =
        gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    combo_apparent_temperature_set_tooltip(combo);
    update_scrollbox(dialog->pd, TRUE);
    update_summary_window(dialog, TRUE);
}


static void
create_units_page(xfceweather_dialog *dialog)
{
    /* temperature */
    dialog->combo_unit_temperature = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_unit_temperature"));

    /* atmospheric pressure */
    dialog->combo_unit_pressure = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_unit_pressure"));

    /* wind speed */
    dialog->combo_unit_windspeed = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_unit_windspeed"));

    /* precipitation */
    dialog->combo_unit_precipitation = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_unit_precipitation"));

    /* altitude */
    dialog->combo_unit_altitude = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_unit_altitude"));

    /* apparent temperature model */
    dialog->combo_apparent_temperature = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_apparent_temperature"));

    /* initialize widgets with current data */
    if (dialog->pd)
        setup_units(dialog, dialog->pd->units);
}


static void
combo_icon_theme_set_tooltip(GtkWidget *combo,
                             gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    gchar *text;

    if (G_UNLIKELY(dialog->pd->icon_theme == NULL)) {
        /* this should never happen, but who knows... */
        gtk_widget_set_tooltip_text(GTK_WIDGET(combo),
                                    _("Choose an icon theme."));
        return;
    }

    text = g_strdup_printf
        (_("<b>Directory:</b> %s\n\n"
           "<b>Author:</b> %s\n\n"
           "<b>Description:</b> %s\n\n"
           "<b>License:</b> %s"),
         TEXT_UNKNOWN(dialog->pd->icon_theme->dir),
         TEXT_UNKNOWN(dialog->pd->icon_theme->author),
         TEXT_UNKNOWN(dialog->pd->icon_theme->description),
         TEXT_UNKNOWN(dialog->pd->icon_theme->license));
    gtk_widget_set_tooltip_markup(GTK_WIDGET(combo), text);
    g_free(text);
}


static void
combo_icon_theme_changed(GtkWidget *combo,
                         gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    icon_theme *theme;
    gint i;

    i = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (G_UNLIKELY(i == -1))
        return;

    theme = g_array_index(dialog->icon_themes, icon_theme *, i);
    if (G_UNLIKELY(theme == NULL))
        return;

    icon_theme_free(dialog->pd->icon_theme);
    dialog->pd->icon_theme = icon_theme_copy(theme);
    combo_icon_theme_set_tooltip(combo, dialog);
    update_icon(dialog->pd);
    update_summary_window(dialog, TRUE);
}


static void
button_icons_dir_clicked(GtkWidget *button,
                         gpointer user_data)
{
    gchar *dir, *command;

    dir = get_user_icons_dir();
    g_mkdir_with_parents(dir, 0755);
    command = g_strdup_printf("exo-open %s", dir);
    g_free(dir);
    xfce_spawn_command_line_on_screen(gdk_screen_get_default(),
                                      command, FALSE, TRUE, NULL);
    g_free(command);
}


static void
check_single_row_toggled(GtkWidget *button,
                         gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->single_row =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
#if LIBXFCE4PANEL_CHECK_VERSION(4,9,0)
    xfceweather_set_mode(dialog->pd->plugin,
                         xfce_panel_plugin_get_mode(dialog->pd->plugin),
                         dialog->pd);
#endif
}


static void
combo_tooltip_style_changed(GtkWidget *combo,
                            gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->tooltip_style = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
}


static void
combo_forecast_layout_set_tooltip(GtkWidget *combo)
{
    gchar *text = NULL;
    gint value = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));

    switch (value) {
    case FC_LAYOUT_CALENDAR:
        text = _("A more calendar-like view, with the days in columns and the "
                 "daytimes (morning, afternoon, evening, night) in rows.");
        break;
    case FC_LAYOUT_LIST:
        text = _("Shows the forecasts in a table with the daytimes (morning, "
                 "afternoon, evening, night) in columns and the days in rows.");
        break;
    }
    gtk_widget_set_tooltip_markup(GTK_WIDGET(combo), text);
}


static void
combo_forecast_layout_changed(GtkWidget *combo,
                              gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->forecast_layout =
        gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    combo_forecast_layout_set_tooltip(combo);
    update_summary_window(dialog, FALSE);
}


static void
spin_forecast_days_value_changed(const GtkWidget *spin,
                                 gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->forecast_days =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
    update_summary_window(dialog, FALSE);
}


static void
check_round_values_toggled(GtkWidget *button,
                           gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->round =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
    update_scrollbox(dialog->pd, TRUE);
    update_summary_window(dialog, TRUE);
}


static void
create_appearance_page(xfceweather_dialog *dialog)
{
    icon_theme *theme;
    gchar *text;
    guint i;

    /* icon theme */
    dialog->combo_icon_theme = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_icon_theme"));
    dialog->button_icons_dir = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "button_icons_dir"));

    dialog->icon_themes = find_icon_themes();
    for (i = 0; i < dialog->icon_themes->len; i++) {
        theme = g_array_index(dialog->icon_themes, icon_theme *, i);
        ADD_COMBO_VALUE(dialog->combo_icon_theme, theme->name);
        /* set selection to current theme */
        if (G_LIKELY(dialog->pd->icon_theme) &&
            !strcmp(theme->dir, dialog->pd->icon_theme->dir)) {
            SET_COMBO_VALUE(dialog->combo_icon_theme, i);
            combo_icon_theme_set_tooltip(dialog->combo_icon_theme, dialog);
        }
    }

    /* always use small icon in panel */
    dialog->check_single_row = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "check_single_row"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->check_single_row),
                                 dialog->pd->single_row);

    /* tooltip style */
    dialog->combo_tooltip_style = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_tooltip_style"));
    SET_COMBO_VALUE(dialog->combo_tooltip_style, dialog->pd->tooltip_style);

    /* forecast layout */
    dialog->combo_forecast_layout = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "combo_forecast_layout"));
    SET_COMBO_VALUE(dialog->combo_forecast_layout,
                    dialog->pd->forecast_layout);
    combo_forecast_layout_set_tooltip(dialog->combo_forecast_layout);

    /* number of days shown in forecast */
    dialog->spin_forecast_days = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "spin_forecast_days"));
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (dialog->spin_forecast_days), 1, MAX_FORECAST_DAYS);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->spin_forecast_days),
                               (dialog->pd->forecast_days ? dialog->pd->forecast_days : 5));
    text = g_strdup_printf
        (_("Met.no provides forecast data for up to %d days in the "
           "future. Choose how many days will be shown in the forecast "
           "tab in the summary window. On slower computers, a lower "
           "number might help against lags when opening the window. "
           "Note however that usually forecasts for more than three "
           "days in the future are unreliable at best ;-)"),
         MAX_FORECAST_DAYS);
    SET_TOOLTIP(dialog->spin_forecast_days, text);
    g_free(text);

    /* round temperature */
    dialog->check_round_values = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "check_round_values"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dialog->check_round_values),
                                 dialog->pd->round);
}


static void
check_scrollbox_show_toggled(GtkWidget *button,
                             gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->show_scrollbox =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
    scrollbox_set_visible(dialog->pd);
}


static void
spin_scrollbox_lines_value_changed(const GtkWidget *spin,
                                   gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->scrollbox_lines =
        gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin));
    update_scrollbox(dialog->pd, TRUE);
}


static gboolean
button_scrollbox_font_pressed(GtkWidget *button,
                              GdkEventButton *event,
                              gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;

    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;

    if (event->button == 2) {
        g_free(dialog->pd->scrollbox_font);
        dialog->pd->scrollbox_font = NULL;
        gtk_scrollbox_set_fontname(GTK_SCROLLBOX(dialog->pd->scrollbox),
                                   NULL);
        gtk_button_set_label(GTK_BUTTON(button), _("Select _font"));
        return TRUE;
    }

    return FALSE;
}

static gboolean
button_scrollbox_font_clicked(GtkWidget *button,
                              gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    GtkFontChooserDialog *fsd;
    gchar *fontname;
    gint result;

    fsd = GTK_FONT_CHOOSER_DIALOG
        (gtk_font_chooser_dialog_new(_("Select font"), GTK_WINDOW (dialog->dialog)));
    if (dialog->pd->scrollbox_font)
        gtk_font_chooser_set_font (GTK_FONT_CHOOSER (fsd), dialog->pd->scrollbox_font);

    result = gtk_dialog_run(GTK_DIALOG(fsd));
    if (result == GTK_RESPONSE_OK || result == GTK_RESPONSE_ACCEPT) {
        fontname = gtk_font_chooser_get_font (GTK_FONT_CHOOSER (fsd));
        if (fontname != NULL) {
            gtk_button_set_label(GTK_BUTTON(button), fontname);
            g_free(dialog->pd->scrollbox_font);
            dialog->pd->scrollbox_font = fontname;
            gtk_scrollbox_set_fontname(GTK_SCROLLBOX(dialog->pd->scrollbox),
                                       fontname);
        }
    }
    gtk_widget_destroy(GTK_WIDGET(fsd));
    return FALSE;
}


static gboolean
button_scrollbox_color_pressed(GtkWidget *button,
                               GdkEventButton *event,
                               gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;

    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;

    if (event->button == 2) {
        dialog->pd->scrollbox_use_color = FALSE;
        gtk_scrollbox_clear_color(GTK_SCROLLBOX(dialog->pd->scrollbox));
        return TRUE;
    }

    return FALSE;
}


static void
button_scrollbox_color_set(GtkWidget *button,
                           gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;

    gtk_color_chooser_get_rgba(GTK_COLOR_CHOOSER(button),
                               &(dialog->pd->scrollbox_color));
    gtk_scrollbox_set_color(GTK_SCROLLBOX(dialog->pd->scrollbox),
                            dialog->pd->scrollbox_color);
    dialog->pd->scrollbox_use_color = TRUE;
}


static void
update_scrollbox_labels(xfceweather_dialog *dialog)
{
    GtkTreeIter iter;
    gboolean hasiter;
    GValue value = { 0 };
    gint option;

    dialog->pd->labels = labels_clear(dialog->pd->labels);
    hasiter =
        gtk_tree_model_get_iter_first(GTK_TREE_MODEL
                                      (dialog->model_datatypes),
                                      &iter);
    while (hasiter == TRUE) {
        gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model_datatypes),
                                 &iter, 1, &value);
        option = g_value_get_int(&value);
        g_array_append_val(dialog->pd->labels, option);
        g_value_unset(&value);
        hasiter =
            gtk_tree_model_iter_next(GTK_TREE_MODEL(dialog->model_datatypes),
                                     &iter);
    }
    update_scrollbox(dialog->pd, TRUE);
}


static void
add_model_option(GtkListStore *model,
                 const gint opt)
{
    GtkTreeIter iter;

    gtk_list_store_append(model, &iter);
    gtk_list_store_set(model, &iter,
                       0, _(labeloptions[opt].name),
                       1, labeloptions[opt].number, -1);
}


static int
option_i(const data_types opt)
{
    gint i;

    for (i = 0; i < OPTIONS_N; i++)
        if (labeloptions[i].number == opt)
            return i;

    return -1;
}


/* set tooltip according to selection */
static void
options_datatypes_set_tooltip(GtkWidget *optmenu)
{
    gint history, opt = OPTIONS_N;
    
    /* TRANSLATORS: Fallback value, usually never shown. */
    gchar *text = _("Choose the value to add to the list below. "
                    "Values can be added more than once.");

    history = gtk_combo_box_get_active (GTK_COMBO_BOX (optmenu));
    if (G_LIKELY(history > -1) && history < OPTIONS_N)
        opt = labeloptions[history].number;

    switch (opt) {
    case TEMPERATURE:
        text = _("Air temperature, sometimes referred to as dry-bulb "
                 "temperature. Measured by a thermometer that is freely "
                 "exposed to the air, yet shielded from radiation and "
                 "moisture.");
        break;
    case PRESSURE:
        text = _("The weight of the air that makes up the atmosphere exerts "
                 "a pressure on the surface of the Earth, which is known as "
                 "atmospheric pressure. To make it easier to compare the "
                 "value to other values for locations with different "
                 "altitudes, atmospheric pressure is adjusted to the "
                 "equivalent sea-level pressure and called barometric "
                 "pressure. Rising barometric pressures generally indicate "
                 "an improvement in weather conditions, while falling "
                 "pressures mean deterioration.");
        break;
    case WIND_SPEED:
        text = _("Nowadays wind speed/velocity is measured using an "
                 "anemometer (Greek <i>anemos</i>, meaning <i>wind</i>) in "
                 "10 m (33 ft) height. Anemometers usually measure either "
                 "wind speed or pressure, but will provide both values as "
                 "they are closely related to and can be deduced from each "
                 "other.");
        break;
    case WIND_BEAUFORT:
        text = _("Invented by Sir Francis Beaufort in 1805, this empirical "
                 "scale on wind speed is based on people's observations "
                 "of specific land or sea conditions, denoting these "
                 "conditions with numbers from 0 (calm) to 12 (hurricane).");
        break;
    case WIND_DIRECTION:
        text = _("This gives the cardinal direction (North, East, South, "
                 "West) the wind is coming from.");
        break;
    case WIND_DIRECTION_DEG:
        text = _("This gives the direction the wind is coming from "
                 "in azimuth degrees (North = 0°, East = 90°, South = 180° "
                 "and West = 270°).");
        break;
    case HUMIDITY:
        text = _("Humidity is defined as the amount of water vapor in the "
                 "air and increases the possibility of precipitation, fog "
                 "and dew. While absolute humidity is the water content of "
                 "air, relative humidity gives (in %) the current absolute "
                 "humidity relative to the maximum for that air temperature "
                 "and pressure.");
        break;
    case DEWPOINT:
        text = _("This is the temperature to which air must be cooled to "
                 "reach 100% relative humidity, given no change in water "
                 "content. Reaching the dew point halts the cooling process, "
                 "as condensation occurs which releases heat into the air. "
                 "A high dew point increases the possibility of rain and "
                 "severe thunderstorms. The dew point allows the prediction "
                 "of dew, frost, fog and minimum overnight temperature, and "
                 "has influence on the comfort level one experiences.\n\n"
                 "<b>Note:</b> This is a calculated value not provided by "
                 "met.no.");
        break;
    case APPARENT_TEMPERATURE:
        text = _("Also known as <i>felt temperature</i>, <i>effective "
                 "temperature</i>, or what some weather providers declare as "
                 "<i>feels like</i>. Human temperature sensation is not only "
                 "based on air temperature, but also on heat flow, physical "
                 "activity and individual condition. While being a highly "
                 "subjective value, apparent temperature can actually be "
                 "useful for warning about extreme conditions (cold, "
                 "heat).\n\n"
                 "<b>Note:</b> This is a calculated value not provided by "
                 "met.no. You should use a calculation model appropriate for "
                 "your local climate and personal preferences on the units "
                 "page.");
        break;
    case CLOUDS_LOW:
        text = _("This gives the low-level cloud cover in percent. According "
                 "to WMO definition, low-level clouds can be found at "
                 "altitudes below 4,000 m (13,000 ft), or 5,000 m (16,000 ft) "
                 "at the equator, though their basis often lie below 2,000 m "
                 "(6,500 ft). They are mainly composed of water droplets or "
                 "ice particles and snow, when temperatures are cold enough.");
        break;
    case CLOUDS_MID:
        text = _("This specifies the mid-level cloud cover in percent. "
                 "According to WMO definition, mid-level clouds form in "
                 "heights of 4,000-8,000 m (13,000-26,000 ft), or 5,000-"
                 "10,000 m (16,000-33,000 ft) at the equator. Like their "
                 "low-level cousins, they are principally composed of water "
                 "droplets. When temperatures get low enough, ice particles "
                 "can replace the droplets.");
        break;
    case CLOUDS_HIGH:
        text = _("This reports the high-level cloud cover in percent. "
                 "According to WMO definition, high-level clouds can be found "
                 "in altitudes of 8,000 to 15,000 m (26,000 to 49,000 ft), or "
                 "10,000 m-18,000 m (33,000-59,000 ft) at the equator, where "
                 "temperatures are so low that they are mainly composed of "
                 "ice crystals. While typically thin and white in appearance, "
                 "they can be seen in a magnificent array of colors when the "
                 "sun is low on the horizon.");
        break;
    case CLOUDINESS:
        text = _("Cloudiness, or cloud cover, defines the fraction of the "
                 "sky obscured by clouds when observed from a given "
                 "location. Clouds are both carriers of precipitation and "
                 "regulator to the amount of solar radiation that reaches "
                 "the surface. While during daytime they reduce the "
                 "temperature, at night they have the opposite effect, as "
                 "water vapor prevents long-wave radiation from escaping "
                 "into space. Apart from that, clouds reflect light to "
                 "space and in that way contribute to the cooling of the "
                 "planet.");
        break;
    case FOG:
        text = _("Fog is a type of low-lying stratus cloud, with the "
                 "moisture in it often generated locally such as from a "
                 "nearby lake, river, ocean, or simply moist ground, "
                 "that forms when the difference between temperature "
                 "and dew point is below 2.5 °C (4 °F), usually at a "
                 "relative humidity of 100%. Fog commonly produces "
                 "precipitation in the form of drizzle or very light "
                 "snow and reduces visibility to less than 1 km "
                 "(5/8 statute mile).");
        break;
    case PRECIPITATION:
        text = _("The amount of rain, drizzle, sleet, hail, snow, graupel "
                 "and other forms of water falling from the sky over a "
                 "specific period.\n\n"
                 "The values reported by met.no are those of precipitation "
                 "in the liquid state - or in other words: of rain -, so if "
                 "snow is expected (but not sleet), then the amount of snow "
                 "will be <i>guessed</i> by multiplying the original value by "
                 "a ratio dependent on the air temperature:\n\n<tt><small>"
                 "                   T &lt; -11.1 °C (12 °F) =&gt; 1:12\n"
                 "-11.1 °C (12 °F) &lt; T &lt;  -4.4 °C (24 °F) =&gt; 1:10\n"
                 " -4.4 °C (24 °F) &lt; T &lt;  -2.2 °C (28° F) =&gt; 1:7\n"
                 " -2.2 °C (28 °F) &lt; T &lt;  -0.6 °C (31 °F) =&gt; 1:5\n"
                 " -0.6 °C (31 °F) &lt; T                    =&gt; 1:3\n\n"
                 "</small></tt>"
                 "Example: If temperature is -5 °C (12 °F), then snow "
                 "density will be low and a rain to snow ratio of 1:10 will "
                 "be used for calculation. Assuming the reported value is "
                 "5 mm, then the calculated amount of snow precipitation is "
                 "50 mm.\n\n"
                 "<b>Note</b>: While air temperature is an important factor "
                 "in this calculation, there are other influencing factors "
                 "that the plugin doesn't know about like the type of snow "
                 "and ground temperature. Because of that, these rules will "
                 "only lead to rough estimates and may not represent the "
                 "real amount of snow.");
        break;
    }

    gtk_widget_set_tooltip_markup(GTK_WIDGET(optmenu), text);
}


static void
options_datatypes_changed(GtkWidget *optmenu,
                          gpointer user_data)
{
    options_datatypes_set_tooltip(optmenu);
}


static gboolean
button_add_option_clicked(GtkWidget *widget,
                          gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    gint index = gtk_combo_box_get_active (GTK_COMBO_BOX (dialog->options_datatypes));
    if (index >= 0) {
        add_model_option(dialog->model_datatypes, index);
        update_scrollbox_labels(dialog);
    }
    return FALSE;
}


static gboolean
button_del_option_clicked(GtkWidget *widget,
                          gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    GtkTreeSelection *selection;
    GtkTreeIter iter;

    selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->list_datatypes));
    if (gtk_tree_selection_get_selected(selection, NULL, &iter))
        gtk_list_store_remove(GTK_LIST_STORE(dialog->model_datatypes), &iter);
    update_scrollbox_labels(dialog);
    return FALSE;
}


static gboolean
button_up_option_clicked(GtkWidget *widget,
                         gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    GtkTreeSelection *selection;
    GtkTreeIter iter, prev;
    GtkTreePath *path;

    selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->list_datatypes));
    if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(dialog->model_datatypes),
                                       &iter);
        if (gtk_tree_path_prev(path)) {
            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model_datatypes),
                                        &prev, path))
                gtk_list_store_move_before(GTK_LIST_STORE(dialog->model_datatypes),
                                           &iter, &prev);

            gtk_tree_path_free(path);
        }
    }
    update_scrollbox_labels(dialog);
    return FALSE;
}


static gboolean
button_down_option_clicked(GtkWidget *widget,
                           gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    GtkTreeIter iter, next;
    GtkTreeSelection *selection;

    selection =
        gtk_tree_view_get_selection(GTK_TREE_VIEW(dialog->list_datatypes));
    if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
        next = iter;
        if (gtk_tree_model_iter_next(GTK_TREE_MODEL(dialog->model_datatypes),
                                     &next))
            gtk_list_store_move_after(GTK_LIST_STORE(dialog->model_datatypes),
                                      &iter, &next);
    }
    update_scrollbox_labels(dialog);
    return FALSE;
}


static void
check_scrollbox_animate_toggled(GtkWidget *button,
                                gpointer user_data)
{
    xfceweather_dialog *dialog = (xfceweather_dialog *) user_data;
    dialog->pd->scrollbox_animate =
        gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button));
#ifdef HAVE_UPOWER_GLIB
    if (dialog->pd->upower_on_battery)
        gtk_scrollbox_set_animate(GTK_SCROLLBOX(dialog->pd->scrollbox),
                                  FALSE);
    else
#endif
        gtk_scrollbox_set_animate(GTK_SCROLLBOX(dialog->pd->scrollbox),
                                  dialog->pd->scrollbox_animate);
}


static void
create_scrollbox_page(xfceweather_dialog *dialog)
{
    GtkWidget *button;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    data_types type;
    guint i;
    gint n;

    /* show scrollbox */
    dialog->check_scrollbox_show = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "check_scrollbox_show"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                 (dialog->check_scrollbox_show),
                                 dialog->pd->show_scrollbox);

    /* values to show at once (multiple lines) */
    dialog->spin_scrollbox_lines = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "spin_scrollbox_lines"));
    gtk_spin_button_set_range (GTK_SPIN_BUTTON (dialog->spin_scrollbox_lines), 1, MAX_SCROLLBOX_LINES);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (dialog->spin_scrollbox_lines), dialog->pd->scrollbox_lines);

    /* font and color */
    dialog->button_scrollbox_font = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "button_scrollbox_font"));
    if (dialog->pd->scrollbox_font)
        gtk_button_set_label(GTK_BUTTON(dialog->button_scrollbox_font),
                             dialog->pd->scrollbox_font);
    dialog->button_scrollbox_color = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "button_scrollbox_color"));
    gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (dialog->button_scrollbox_color), &(dialog->pd->scrollbox_color));

    /* labels and buttons */
    dialog->options_datatypes = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "options_datatypes"));
    options_datatypes_set_tooltip(dialog->options_datatypes);
    dialog->model_datatypes = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    dialog->list_datatypes = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "list_datatypes"));
    gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->list_datatypes), GTK_TREE_MODEL(dialog->model_datatypes));
    renderer = gtk_cell_renderer_text_new();
    column =
        gtk_tree_view_column_new_with_attributes(_("Labels to d_isplay"),
                                                 renderer, "text", 0, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(dialog->list_datatypes), column);

    /* button "add" */
    button = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "button_add"));
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_add_option_clicked), dialog);

    /* button "remove" */
    button = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "button_del"));
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_del_option_clicked), dialog);

    /* button "move up" */
    button = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "button_up"));
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_up_option_clicked), dialog);

    /* button "move down" */
    button = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "button_down"));
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(button_down_option_clicked), dialog);

    if (dialog->pd->labels->len > 0) {
        for (i = 0; i < dialog->pd->labels->len; i++) {
            type = g_array_index(dialog->pd->labels, data_types, i);

            if ((n = option_i(type)) != -1)
                add_model_option(dialog->model_datatypes, n);
        }
    }

    dialog->check_scrollbox_animate = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (dialog->builder), "check_scrollbox_animate"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                 (dialog->check_scrollbox_animate),
                                 dialog->pd->scrollbox_animate);
}


static void
notebook_page_switched(GtkNotebook *notebook,
                       GtkWidget *page,
                       guint page_num,
                       gpointer user_data)
{
    plugin_data *data = (plugin_data *) user_data;

    data->config_remember_tab = page_num;
}


static void
setup_units(xfceweather_dialog *dialog,
            const units_config *units)
{
    if (G_UNLIKELY(units == NULL))
        return;

    SET_COMBO_VALUE(dialog->combo_unit_temperature, units->temperature);
    combo_unit_temperature_set_tooltip(dialog->combo_unit_temperature);
    SET_COMBO_VALUE(dialog->combo_unit_pressure, units->pressure);
    combo_unit_pressure_set_tooltip(dialog->combo_unit_pressure);
    SET_COMBO_VALUE(dialog->combo_unit_windspeed, units->windspeed);
    combo_unit_windspeed_set_tooltip(dialog->combo_unit_windspeed);
    SET_COMBO_VALUE(dialog->combo_unit_precipitation, units->precipitation);
    combo_unit_precipitation_set_tooltip(dialog->combo_unit_precipitation);
    SET_COMBO_VALUE(dialog->combo_unit_altitude, units->altitude);
    combo_unit_altitude_set_tooltip(dialog->combo_unit_altitude);
    SET_COMBO_VALUE(dialog->combo_apparent_temperature,
                    units->apparent_temperature);
    combo_apparent_temperature_set_tooltip(dialog->combo_apparent_temperature);
}


static void
setup_notebook_signals(xfceweather_dialog *dialog)
{
    /* location page */
    g_signal_connect(GTK_EDITABLE(dialog->text_loc_name), "changed",
                     G_CALLBACK(text_loc_name_changed), dialog);
    g_signal_connect(GTK_SPIN_BUTTON(dialog->spin_lat), "value-changed",
                     G_CALLBACK(spin_lat_value_changed), dialog);
    g_signal_connect(GTK_SPIN_BUTTON(dialog->spin_lon), "value-changed",
                     G_CALLBACK(spin_lon_value_changed), dialog);
    g_signal_connect(GTK_SPIN_BUTTON(dialog->spin_alt), "value-changed",
                     G_CALLBACK(spin_alt_value_changed), dialog);
    g_signal_connect(GTK_EDITABLE(dialog->text_timezone), "changed",
                     G_CALLBACK(text_timezone_changed), dialog);

    /* units page */
    g_signal_connect(dialog->combo_unit_temperature, "changed",
                     G_CALLBACK(combo_unit_temperature_changed), dialog);
    g_signal_connect(dialog->combo_unit_pressure, "changed",
                     G_CALLBACK(combo_unit_pressure_changed), dialog);
    g_signal_connect(dialog->combo_unit_windspeed, "changed",
                     G_CALLBACK(combo_unit_windspeed_changed), dialog);
    g_signal_connect(dialog->combo_unit_precipitation, "changed",
                     G_CALLBACK(combo_unit_precipitation_changed), dialog);
    g_signal_connect(dialog->combo_unit_altitude, "changed",
                     G_CALLBACK(combo_unit_altitude_changed), dialog);
    g_signal_connect(dialog->combo_apparent_temperature, "changed",
                     G_CALLBACK(combo_apparent_temperature_changed), dialog);

    /* appearance page */
    g_signal_connect(dialog->combo_icon_theme, "changed",
                     G_CALLBACK(combo_icon_theme_changed), dialog);
    g_signal_connect(dialog->button_icons_dir, "clicked",
                     G_CALLBACK(button_icons_dir_clicked), dialog);
    g_signal_connect(dialog->check_single_row, "toggled",
                     G_CALLBACK(check_single_row_toggled), dialog);
    g_signal_connect(dialog->combo_tooltip_style, "changed",
                     G_CALLBACK(combo_tooltip_style_changed), dialog);
    g_signal_connect(dialog->combo_forecast_layout, "changed",
                     G_CALLBACK(combo_forecast_layout_changed), dialog);
    g_signal_connect(GTK_SPIN_BUTTON(dialog->spin_forecast_days),
                     "value-changed",
                     G_CALLBACK(spin_forecast_days_value_changed), dialog);
    g_signal_connect(dialog->check_round_values, "toggled",
                     G_CALLBACK(check_round_values_toggled), dialog);

    /* scrollbox page */
    g_signal_connect(dialog->check_scrollbox_show, "toggled",
                     G_CALLBACK(check_scrollbox_show_toggled), dialog);
    g_signal_connect(GTK_SPIN_BUTTON(dialog->spin_scrollbox_lines),
                     "value-changed",
                     G_CALLBACK(spin_scrollbox_lines_value_changed),
                     dialog);
    g_signal_connect(G_OBJECT(dialog->button_scrollbox_font),
                     "button_press_event",
                     G_CALLBACK(button_scrollbox_font_pressed),
                     dialog);
    g_signal_connect(dialog->button_scrollbox_font, "clicked",
                     G_CALLBACK(button_scrollbox_font_clicked), dialog);
    g_signal_connect(G_OBJECT(dialog->button_scrollbox_color),
                     "button-press-event",
                     G_CALLBACK(button_scrollbox_color_pressed),
                     dialog);
    g_signal_connect(dialog->button_scrollbox_color, "color-set",
                     G_CALLBACK(button_scrollbox_color_set), dialog);
    g_signal_connect(dialog->options_datatypes, "changed",
                     G_CALLBACK(options_datatypes_changed), dialog);
    g_signal_connect(dialog->check_scrollbox_animate, "toggled",
                     G_CALLBACK(check_scrollbox_animate_toggled), dialog);

    /* notebook widget */
    gtk_widget_show_all(GTK_WIDGET(dialog->notebook));
    gtk_notebook_set_current_page(GTK_NOTEBOOK(dialog->notebook),
                                  dialog->pd->config_remember_tab);
    g_signal_connect(GTK_NOTEBOOK(dialog->notebook), "switch-page",
                     G_CALLBACK(notebook_page_switched), dialog->pd);
}


xfceweather_dialog *
create_config_dialog(plugin_data *data,
                     GtkBuilder  *builder)
{
    xfceweather_dialog *dialog;

    dialog = g_slice_new0(xfceweather_dialog);
    dialog->pd = (plugin_data *) data;
    dialog->dialog = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (builder), "dialog"));
    dialog->builder = builder;

    dialog->notebook = GTK_WIDGET (gtk_builder_get_object (GTK_BUILDER (builder), "notebook"));
    create_location_page(dialog);
    create_units_page(dialog);
    create_appearance_page(dialog);
    create_scrollbox_page(dialog);

    setup_notebook_signals(dialog);

    /* automatically detect current location if it is yet unknown */
    if (!(dialog->pd->lat && dialog->pd->lon))
        start_auto_locate(dialog);

    return dialog;
}
