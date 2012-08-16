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

#include <string.h>
#include <time.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"
#include "weather-translate.h"

#define DAY_LOC_N (sizeof(gchar) * 100)


static const gchar *wdirs[] = {
    /* TRANSLATORS: Wind directions. It's where the wind comes _from_. */
    N_("S"),
    N_("SSW"),
    N_("SW"),
    N_("WSW"),
    N_("W"),
    N_("WNW"),
    N_("NW"),
    N_("NNW"),
    N_("N"),
    N_("NNE"),
    N_("NE"),
    N_("ENE"),
    N_("E"),
    N_("ESE"),
    N_("SE"),
    N_("SSE"),
    N_("CALM"),
    NULL
};

static const gchar *moon_phases[] = {
    /* TRANSLATORS: Moon phases */
    N_("New moon"),
    N_("Waxing crescent"),
    N_("First quarter"),
    N_("Waxing gibbous"),
    N_("Full moon"),
    N_("Waning gibbous"),
    N_("Third quarter"),
    N_("Waning crescent"),
    NULL
};
#define NUM_MOON_PHASES (sizeof(moon_phases) / sizeof(gchar *))

static const gchar *
translate_str(const gchar **loc_strings,
              const gchar *str)
{
    gint loc_string_len, str_len;
    guint i;

    if (str == NULL)
        return "?";

    str_len = strlen(str);

    if (str_len < 1)
        return "?";

    for (i = 0; loc_strings[i] != NULL; i++) {
        loc_string_len = strlen(loc_strings[i]);

        if (str_len != loc_string_len)
            continue;

        if (str[0] != loc_strings[i][0])
            continue;

        if (!g_ascii_strncasecmp(loc_strings[i], str, str_len))
            return _(loc_strings[i]);
    }
    return str;
}


typedef struct {
    gint id;
    gchar *symbol;
    gchar *desc;
    gchar *night_desc;
} symbol_desc;

static const symbol_desc symbol_to_desc[] = {
    /*
     * TRANSLATORS: Some of these terms seem to contradict
     * themselves. If I'm not mistaken, this is because the forecasts
     * are for certain time periods which may be 1, 3 or 6 hours, and
     * during those periods the weather can alternate between several
     * states. For more information, you might want to read
     * http://api.met.no/weatherapi/locationforecastlts/1.1/documentation,
     * but unfortunately it is not very revealing either.
     */
    { 1, "SUN",                 N_("Sunny"),                     N_("Clear")		},
    { 2, "LIGHTCLOUD",          N_("Lightly cloudy"),            N_("Lightly cloudy")	},
    { 3, "PARTLYCLOUD",         N_("Partly cloudy"),             N_("Partly cloudy")	},
    { 4, "CLOUD",               N_("Cloudy"),                    N_("Cloudy")		},
    { 5, "LIGHTRAINSUN",        N_("Sunny, rain showers"),       N_("Clear, rain showers") },
    { 6, "LIGHTRAINTHUNDERSUN", N_("Sunny, rain showers with thunder"),
      N_("Clear, rain showers with thunder") },
    { 7, "SLEETSUN",            N_("Sunny, sleet"),              N_("Clear, sleet") },
    { 8, "SNOWSUN",             N_("Sunny, snow"),               N_("Clear, snow") },
    { 9, "LIGHTRAIN",           N_("Rain showers"),              N_("Rain showers") },
    { 10,"RAIN",                N_("Rain"),                      N_("Rain") },
    { 11,"RAINTHUNDER",         N_("Rain with thunder"),         N_("Rain with thunder") },
    { 12,"SLEET",               N_("Sleet"),                     N_("Sleet") },
    { 13,"SNOW",                N_("Snow"),                      N_("Snow") },
    { 14,"SNOWTHUNDER",         N_("Snow with thunder"),         N_("Snow with thunder") },
    { 15,"FOG",                 N_("Fog"),                       N_("Fog") },
    { 16,"SUN",                 N_("Sunny"),                     N_("Clear") },
    { 17,"LIGHTCLOUD",          N_("Lightly cloudy"),            N_("Lightly cloudy") },
    { 18,"LIGHTRAINSUN",        N_("Sunny, rain showers"),       N_("Clear, rain showers") },
    { 19,"SNOWSUN",             N_("Sunny, Snow"),               N_("Clear, Snow") },
    { 20,"SLEETSUNTHUNDER",     N_("Sunny, Sleet and thunder"),  N_("Clear, sleet and thunder") },
    { 21,"SNOWSUNTHUNDER",      N_("Sunny, Snow and thunder"),   N_("Clear, snow and thunder") },
    { 22,"LIGHTRAINTHUNDER",    N_("Rain showers with thunder"), N_("Rain showers with thunder") },
    { 23,"SLEETTHUNDER",        N_("Sleet and thunder"),         N_("Sleet and thunder") },
};

#define NUM_SYMBOLS (sizeof(symbol_to_desc) / sizeof(symbol_to_desc[0]))


const gchar *
translate_desc(const gchar *desc,
               const gboolean nighttime)
{
    guint i;

    for (i = 0; i < NUM_SYMBOLS; i++)
        if (!strcmp(desc, symbol_to_desc[i].symbol))
            if (nighttime)
                return _(symbol_to_desc[i].night_desc);
            else
                return _(symbol_to_desc[i].desc);
    return desc;
}


const gchar *
translate_moon_phase(const gchar *moon_phase)
{
    guint i;

    for (i = 0; i < NUM_MOON_PHASES; i++)
        if (!strcmp(moon_phase, moon_phases[i]))
            return _(moon_phases[i]);
    return moon_phase;
}


gchar *
translate_day(const gint weekday)
{
    struct tm time_tm;
    gchar *day_loc;
    int len;

    if (weekday < 0 || weekday > 6)
        return NULL;

    time_tm.tm_wday = weekday;

    day_loc = g_malloc(DAY_LOC_N);

    len = strftime(day_loc, DAY_LOC_N, "%A", &time_tm);
    day_loc[len] = 0;
    if (!g_utf8_validate(day_loc, -1, NULL)) {
        gchar *utf8 = g_locale_to_utf8(day_loc, -1, NULL, NULL, NULL);
        g_free(day_loc);
        day_loc = utf8;
    }

    return day_loc;
}


gchar *
translate_wind_direction(const gchar *wdir)
{
    gint wdir_len;
    gchar *wdir_loc, *tmp, wdir_i[2];
    guint i;

    if (wdir == NULL || (wdir_len = strlen(wdir)) < 1)
        return NULL;

    if (strchr(wdir, '/'))        /* N/A */
        return NULL;

    /*
     * If the direction can be translated, then translate the whole
     * code so that it can be correctly translated to CJK (and
     * possibly Finnish). If not, use the old behaviour where
     * individual direction codes are successively translated.
     */
    if (g_ascii_strcasecmp(wdir, _(wdir)) != 0)
        wdir_loc = g_strdup(_(wdir));
    else {
        wdir_loc = g_strdup("");
        for (i = 0; i < strlen(wdir); i++) {
            wdir_i[0] = wdir[i];
            wdir_i[1] = '\0';

            tmp = g_strdup_printf("%s%s", wdir_loc,
                                  translate_str(wdirs, wdir_i));
            g_free(wdir_loc);
            wdir_loc = tmp;
        }

    }
    return wdir_loc;
}


/* Return "calm", "N/A" or a number */
gchar *
translate_wind_speed(const gchar *wspeed,
                     const unit_systems unit_system)
{
    gchar *wspeed_loc;

    if (g_ascii_strcasecmp(wspeed, "calm") == 0)
        wspeed_loc = g_strdup(_("calm"));
    else if (g_ascii_strcasecmp(wspeed, "N/A") == 0)
        wspeed_loc = g_strdup(_("N/A"));
    else {
        wspeed_loc =
            g_strdup_printf("%s %s", wspeed, get_unit(unit_system, WIND_SPEED));
    }
    return wspeed_loc;
}
