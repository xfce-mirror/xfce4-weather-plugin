#include <config.h>

#include "translate.h"

#include <string.h> /* strlen() */
#include <libxfce4util/i18n.h>

const gchar *desc_strings[] = {
        N_("Snow to Rain"),
        N_("Becoming Cloudy"),
        N_("Blizzard"),
        N_("Blizzard Conditions"),
        N_("Blowing Snow"),
        N_("Chance of Rain"),
        N_("Chance of Rain/Snow"),
        N_("Chance of Showers"),
        N_("Chance of Snow"),
        N_("Chance of Snow/Rain"),
        N_("Chance of T-Storm"),
        N_("Clear"),
        N_("Clearing"),
        N_("Clouds"),
        N_("Cloudy"),
        N_("Cloudy Periods"),
        N_("Continued Hot"),
        N_("Cumulonimbus Clouds Observed"),
        N_("Drifting Snow"),
        N_("Drizzle"),
        N_("Dry"),
        N_("Fair"),
        N_("Flurries"),
        N_("Fog"),
        N_("Freezing Drizzle"),
        N_("Freezing Rain"),
        N_("Freezing Rain/Snow"),
        N_("Frozen Precip"),
        N_("Hazy"),
        N_("Heavy Rain"),
        N_("Heavy Snow"),
        N_("Hot And Humid"),
        N_("Ice Crystals"),
        N_("Ice/Snow Mixture"),
        N_("Increasing Clouds"),
        N_("Isolated Showers"),
        N_("Light Rain"),
        N_("Light Snow"),
        N_("Lightning Observed"),
        N_("mild and breezy"),
        N_("Mist"),
        N_("Mostly Clear"),
        N_("Mostly Cloudy"),
        N_("Mostly Sunny"),
        N_("N/A"),
        N_("Occasional Sunshine"),
        N_("Overcast"),
        N_("Partial Clearing"),
        N_("Partial Sunshine"),
        N_("Partly Cloudy"),
        N_("Partly Sunny"),
        N_("Rain"),
        N_("Rain and Snow"),
        N_("Rain or Snow"),
        N_("Rain Showers"),
        N_("Rain to Snow"),
        N_("Rain/Snow Showers"),
        N_("Showers"),
        N_("Sleet"),
        N_("Sleet and Snow"),
        N_("Smoke"),
        N_("Snow"),
        N_("Snow and Rain"),
        N_("Snow or Rain"),
        N_("Snow Showers"),
        N_("Sunny"),
        N_("Thunder"),
        N_("Thunder storms"),
        N_("Variable Cloudiness"),
        N_("Variable Clouds"),
        N_("Windy"),
        N_("Wintry Mix"),
        NULL
};

const gchar *translate_desc(const gchar *desc)
{
        int i;

        if (desc == NULL)
                return "?";

        for (i = 0; desc_strings[i] != NULL; i++)
        {
                if (desc[0] != desc_strings[i][0])
                        continue;

                if (!g_ascii_strncasecmp(desc, desc_strings[i], strlen(desc_strings[i]))) 
                        return _(desc_strings[i]);
        }

        return desc;
}
