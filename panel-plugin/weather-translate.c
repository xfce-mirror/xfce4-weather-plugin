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
#include <time.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"
#include "weather-translate.h"

#define DAY_LOC_N (sizeof(gchar) * 100)
#define NODATA "NODATA"


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

typedef struct {
    gint id;
    gchar *symbol;
    gchar *desc;
    gchar *night_desc;
} symbol_desc;

static const symbol_desc symbol_to_desc[] = {
    /*
     * TRANSLATORS: How these symbols are named and defined is explained at
     * http://om.yr.no/forklaring/symbol/ and https://api.met.no/faq.html#symbols.
     * To be more concise / shorter, the plugin uses names that deviate a bit from yr.no, so that
     * they fit well in the tooltip, forecast tab etc.
     *
     * More information can be obtained from the following pages:
     * http://www.theweathernetwork.com/weathericons/?product=weathericons&pagecontent=index
     * http://www.theweathernetwork.com/index.php?product=help&placecode=cabc0164&pagecontent=helpicons
     * http://www.mir-co.net/sonstiges/wetterausdruecke.htm
     * The latter page is in German, but it contains a symbol table with Norwegian descriptions.
     */
    {  1, "SUN",                 N_("Sunny"),                      N_("Clear")                      },
    {  2, "LIGHTCLOUD",          N_("Lightly cloudy"),             N_("Lightly cloudy")             },
    {  3, "PARTLYCLOUD",         N_("Partly cloudy"),              N_("Partly cloudy")              },
    {  4, "CLOUD",               N_("Cloudy"),                     N_("Cloudy")                     },

    /*
     * http://www.theweathernetwork.com/weathericons/?product=weathericons&pagecontent=index:
     *   "Showers – Some sun is expected, interspersed with showers from
     *    time to time. As opposed to rain, showers describe liquid
     *    precipitation that can vary greatly in intensity over a short
     *    amount of time. [...] Precipitation may be locally heavy for
     *    short amounts of time."
     */
    {  5, "LIGHTRAINSUN",        N_("Rain showers"),               N_("Rain showers")               },

    /*
     * http://www.theweathernetwork.com/weathericons/?product=weathericons&pagecontent=index:
     *   "Thunder Showers - Intermittent rain showers with thunder and lightning, generally
     *    short-lived."
     */
    {  6, "LIGHTRAINTHUNDERSUN", N_("Thunder showers"),            N_("Thunder showers")            },

    /* Analogues to "Rain showers" */
    {  7, "SLEETSUN",            N_("Sleet showers"),              N_("Sleet showers")              },
    {  8, "SNOWSUN",             N_("Snow showers"),               N_("Snow showers")               },

    /* It's raining, usually incessantly, but not heavily. */
    {  9, "LIGHTRAIN",           N_("Light rain"),                 N_("Light rain")                 },

    /* Heavy, usually incessant rain. met.no now uses "heavy rain", but personally I find light
     * rain and rain somewhat better. Symbol names indicate this is the way met.no did it some
     * time ago. */
    { 10, "RAIN",                N_("Rain"),                       N_("Rain")                       },

    { 11, "RAINTHUNDER",         N_("Rain with thunder"),          N_("Rain with thunder")          },

    /* Sleet is a mixture of rain and snow, but it's not hail. */
    { 12, "SLEET",               N_("Sleet"),                      N_("Sleet")                      },

    { 13, "SNOW",                N_("Snow"),                       N_("Snow")                       },

    /*
     * http://en.wikipedia.org/wiki/Thundersnow:
     *   "Thundersnow, also known as a winter thunderstorm or a thunder snowstorm, is a relatively
     *    rare kind of thunderstorm with snow falling as the primary precipitation instead of
     *    rain. It typically falls in regions of strong upward motion within the cold sector of an
     *    extratropical cyclone."
     */
    { 14, "SNOWTHUNDER",         N_("Thundersnow"),                N_("Thundersnow")                },

    { 15, "FOG",                 N_("Fog"),                        N_("Fog")                        },

    /* Symbols 16-19 are used for polar days (unused beginning with API version 1.2) */
    { 16, "SUN",                 N_("Sunny"),                      N_("Clear")                      },
    { 17, "LIGHTCLOUD",          N_("Lightly cloudy"),             N_("Lightly cloudy")             },
    { 18, "LIGHTRAINSUN",        N_("Rain showers"),               N_("Rain showers")               },
    { 19, "SNOWSUN",             N_("Snow showers"),               N_("Snow showers")               },

    /* Same as symbols 1-15, but with thunder */
    { 20, "SLEETSUNTHUNDER",     N_("Sleet showers with thunder"), N_("Sleet showers with thunder") },
    { 21, "SNOWSUNTHUNDER",      N_("Snow showers with thunder"),  N_("Snow showers with thunder")  },
    { 22, "LIGHTRAINTHUNDER",    N_("Light rain with thunder"),    N_("Light rain with thunder")    },
    { 23, "SLEETTHUNDER",        N_("Sleet with thunder"),         N_("Sleet with thunder")         },
};

#define NUM_SYMBOLS (sizeof(symbol_to_desc) / sizeof(symbol_to_desc[0]))


/*
 * API version 1.2, published in May 2014, introduced new symbols. We
 * try to match these with existing symbols, in order to be compatible
 * with existing icon themes and to maintain translation completeness.
 *
 * See https://api.met.no/weatherapi/weathericon/1.1/documentation
 * for a list of symbols. For a list of symbols with descriptions,
 * see http://om.yr.no/forklaring/symbol.
 */
gint
replace_symbol_id(gint id)
{
    /* Symbol ids greater than 100 are used for indicating polar
     * night. These ids are over the ordinary id + 100. Since we
     * don't support polar icons, we can simply subtract 100 to
     * get the non-polar symbol ids.
     */
    if (id > 100)
        id -= 100;

    switch (id) {
    case 24: return 22; /* Light rain showers and thunder */
    case 25: return 6;  /* Heavy rain showers and thunder */
    case 26: return 20; /* Light sleet showers and thunder */
    case 27: return 20; /* Heavy sleet showers and thunder */
    case 28: return 21; /* Light snow showers and thunder */
    case 29: return 21; /* Heavy snow showers and thunder */
    case 30: return 22; /* Light rain and thunder */
    case 31: return 23; /* Light sleet and thunder */
    case 32: return 23; /* Heavy sleet and thunder */
    case 33: return 14; /* Light snow and thunder */
    case 34: return 14; /* Heavy snow and thunder */

    /* symbols 35-39 are unused */

    case 40: return 5;  /* Light rain showers */
    case 41: return 5;  /* Heavy rain showers */
    case 42: return 7;  /* Light sleet showers */
    case 43: return 7;  /* Heavy sleet showers */
    case 44: return 8;  /* Light snow showers */
    case 45: return 8;  /* Heavy snow showers */
    case 46: return 9;  /* Light rain */
    case 47: return 12; /* Light sleet */
    case 48: return 12; /* Heavy sleet */
    case 49: return 13; /* Light snow */
    case 50: return 13; /* Heavy snow */
    default: return id;
    }
}


const gchar *
get_symbol_for_id(gint id)
{
    if (G_UNLIKELY(id < 1))
        return NODATA;

    if (id >= NUM_SYMBOLS)
        id = replace_symbol_id(id);

    if (id < NUM_SYMBOLS)
        return symbol_to_desc[id-1].symbol;

    return NODATA;
}


const gchar *
translate_desc(const gchar *desc,
               const gboolean nighttime)
{
    gint i;

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
    gint i;

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
