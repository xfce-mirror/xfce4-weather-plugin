#include <config.h>

#include "translate.h"
#include "debug_print.h"

#include <string.h> /* strlen() */
#include <libxfce4util/i18n.h>
#include <stdlib.h>
#include <time.h>

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
        N_("Rain / Snow Showers"),
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
        N_("Showers in the Vicinity"),
        N_("Light Rain Shower"),
        N_("Showers Late"),
        N_("PM Showers"),
        N_("Light Rain / Wind"),
        N_("Scattered Showers"),
        N_("PM Light Rain"),
        N_("AM Showers"),
        N_("AM Light Rain"),
        N_("Partly Cloudy and Windy"),
        N_("Few Showers"),
        N_("Light Drizzle"),
        N_("Clouds Early / Clearing Late"),
        N_("Mostly Cloudy and Windy"),
        N_("Rain / Snow"),
        N_("Rain and Sleet"),
        N_("Snow Showers Late"),
        N_("Light Drizzle and Windy"),
        N_("Snow Shower"),
        N_("Snow Showers Early"),
        N_("Few Snow Showers"),
        NULL
};

const gchar *bard_strings[] = {
        N_("rising"), 
        N_("steady"), 
        N_("falling"), 
        NULL
};

const gchar *wdirs[] = {
        N_("S"), 
        N_("W"), 
        N_("E"), 
        N_("N"),
        NULL
};

static const gchar *translate_str(
                const gchar **loc_strings, 
                const gchar *str)
{
        int i, loc_string_len, str_len;

        if (str == NULL)
                return "?";

        str_len = strlen(str);

        if (str_len < 1)
                return "?";

        for (i = 0; loc_strings[i] != NULL; i++)
        {
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

const gchar *translate_bard(const gchar *bard)
{
        return translate_str(bard_strings, bard);
}

const gchar *translate_desc(const gchar *desc)
{
        return translate_str(desc_strings, desc);
}

/* used by translate_lsup and translate_time */
static void _fill_time(struct tm *time, 
                const gchar *hour, 
                const gchar *minute,
                const gchar *am)
{
        time->tm_hour = atoi(hour);

        if (am[0] == 'P' && time->tm_hour != 12) /* PM or AM */
                time->tm_hour += 12;
        
        time->tm_min = atoi(minute);
        time->tm_sec = 0;
        time->tm_isdst = -1;
}



#define HDATE_N sizeof(gchar) * 100
gchar *translate_lsup(const gchar *lsup)
{
        char *hdate;
        struct tm time;
        int size = 0, i = 0;
        
        
        gchar **lsup_split; 
        
        if (lsup == NULL || strlen(lsup) == 0)
                return NULL;
        
        /* 10/17/04 5:55 PM Local Time */
        if ((lsup_split = g_strsplit_set(lsup, " /:", 8)) == NULL)
                return NULL;

        while(lsup_split[i++]) 
                size++;
        
        if (size != 8)
        {
                g_strfreev(lsup_split);
                return NULL;
        }

        time.tm_mon = atoi(lsup_split[0]) - 1;
        time.tm_mday = atoi(lsup_split[1]);
        time.tm_year = atoi(lsup_split[2]) + 100;

        _fill_time(&time, lsup_split[3], lsup_split[4], lsup_split[5]);

        g_strfreev(lsup_split);
        
        if (mktime(&time) != -1)
        {
                hdate = g_malloc(HDATE_N);

                strftime(hdate, HDATE_N, _("%x at %X Local Time"), &time);
                
                return hdate;
        }
        else
                return NULL;
}

#define DAY_LOC_N sizeof(gchar) * 20
gchar *translate_day(const gchar *day)
{
        int wday = -1, i;
        const gchar *days[] = {
          "su", "mo", "tu", "we", "th", "fr", "sa", NULL
        };
        gchar *day_loc;
        
        if (day == NULL || strlen(day) < 2)
                return NULL;
        
        for (i = 0; days[i] != NULL; i++)
                if (!g_ascii_strncasecmp(day, days[i], 2))
                        wday = i;

        if (wday == -1)
                return NULL;
        else
        {
                struct tm time;
                
                time.tm_wday = wday;

                day_loc = g_malloc(DAY_LOC_N);

                strftime(day_loc, DAY_LOC_N, "%A", &time);

                return day_loc;
        }
}

/* NNW  VAR */
gchar *translate_wind_direction(const gchar *wdir)
{
        int i, j, wdir_len;
        gchar *wdir_loc;

        if (wdir == NULL || (wdir_len = strlen(wdir)) < 1)
                return NULL;

        if (strchr(wdir, '/')) /* N/A */
                return NULL;
        
        wdir_loc = g_malloc(sizeof(gchar) * (wdir_len + 1));

        for (i = 0; i < strlen(wdir); i++)
                for (j = 0; wdirs[j] != NULL; j++)
                        if (wdir[i] == wdirs[j][0])
                        {
                                gchar *tmp = _(wdirs[j]);
                                
                                wdir_loc[i] = tmp[0];
                        }
        
        wdir_loc[wdir_len] = '\0';
        
        return wdir_loc;
}

/* 8:13 AM */
#define TIME_LOC_N sizeof(gchar) * 20
gchar *translate_time (const gchar *time)
{
        gchar **time_split, *time_loc;
        int i = 0, size = 0;
        struct tm time_tm;

        if (strlen(time) == 0)
                return NULL;

        time_split = g_strsplit_set(time, ": ", 3);

        while(time_split[i++])
                size++;

        if (size != 3)
                return NULL;

        time_loc = g_malloc(TIME_LOC_N);

        _fill_time(&time_tm, time_split[0], time_split[1], time_split[2]);
        g_strfreev(time_split);

        strftime(time_loc, TIME_LOC_N, "%X", &time_tm);

        return time_loc;
}
