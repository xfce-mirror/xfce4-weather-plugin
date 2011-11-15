/*  Copyright (c) 2003-2007 Xfce Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"
#include "weather-translate.h"



#define HDATE_N    (sizeof(gchar) * 100)
#define DAY_LOC_N  (sizeof(gchar) * 100)
#define TIME_LOC_N (sizeof(gchar) * 20)



static const gchar *desc_strings[] = {
  N_("AM Clouds / PM Sun"),
  N_("AM Light Rain"),
  N_("AM Light Snow"),
  N_("AM Rain / Snow Showers"),
  N_("AM Rain / Wind"),
  N_("AM Showers"),
  N_("AM Showers / Wind"),
  N_("AM Snow Showers"),
  N_("AM Snow Showers / Wind"),
  N_("AM T-Storms"),
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
  N_("Clouds Early / Clearing Late"),
  N_("Cloudy"),
  N_("Cloudy / Wind"),
  N_("Cloudy Periods"),
  N_("Continued Hot"),
  N_("Cumulonimbus Clouds Observed"),
  N_("Drifting Snow"),
  N_("Drifting Snow and Windy"),
  N_("Drizzle"),
  N_("Dry"),
  N_("Dust"),
  N_("Fair"),
  N_("Few Showers"),
  N_("Few Snow Showers"),
  N_("Fog"),
  N_("Freezing Drizzle"),
  N_("Freezing Rain"),
  N_("Freezing Rain/Snow"),
  N_("Frigid"),
  N_("Frozen Precip"),
  N_("Hail"),
  N_("Haze"),
  N_("Hazy"),
  N_("Heavy Rain"),
  N_("Heavy Rain Shower"),
  N_("Heavy Snow"),
  N_("Hot And Humid"),
  N_("Hot!"),
  N_("Ice Crystals"),
  N_("Ice/Snow Mixture"),
  N_("Increasing Clouds"),
  N_("Isolated Showers"),
  N_("Isolated T-Storms"),
  N_("Light Drizzle"),
  N_("Light Drizzle and Windy"),
  N_("Light Rain"),
  N_("Light Rain / Wind"),
  N_("Light rain late"),
  N_("Light Rain Shower"),
  N_("Light Snow"),
  N_("Lightning Observed"),
  N_("Mild and Breezy"),
  N_("Mist"),
  N_("Mostly Clear"),
  N_("Mostly Cloudy"),
  N_("Mostly Cloudy / Wind"),
  N_("Mostly Cloudy and Windy"),
  N_("Mostly Sunny"),
  N_("Mostly Sunny / Wind"),
  N_("N/A Not Available"),
  N_("Occasional Sunshine"),
  N_("Overcast"),
  N_("Partial Clearing"),
  N_("Partial Fog"),
  N_("Partial Sunshine"),
  N_("Partly Cloudy"),
  N_("Partly Cloudy / Wind"),
  N_("Partly Cloudy and Windy"),
  N_("Partly Sunny"),
  N_("PM Light Rain"),
  N_("PM Light Snow"),
  N_("PM Rain / Wind"),
  N_("PM Rain / Snow Showers"),
  N_("PM Showers"),
  N_("PM Snow Showers"),
  N_("PM T-Storms"),
  N_("Rain"),
  N_("Rain / Snow"),
  N_("Rain / Snow / Wind"),
  N_("Rain / Snow Late"),
  N_("Rain / Snow Showers"),
  N_("Rain / Snow Showers Early"),
  N_("Rain / Thunder"),
  N_("Rain / Wind"),
  N_("Rain and Sleet"),
  N_("Rain and Snow"),
  N_("Rain or Snow"),
  N_("Rain Shower"),
  N_("Rain Shower and Windy"),
  N_("Rain to snow"),
  N_("Rain/Lightning"),
  N_("Scattered Showers"),
  N_("Scattered Snow Showers"),
  N_("Scattered Snow Showers / Wind"),
  N_("Scattered T-Storms"),
  N_("Showers"),
  N_("Showers / Wind"),
  N_("Showers Early"),
  N_("Showers in the Vicinity"),
  N_("Showers Late"),
  N_("Sleet and Snow"),
  N_("Smoke"),
  N_("Snow"),
  N_("Snow and Rain"),
  N_("Snow or Rain"),
  N_("Snow Shower"),
  N_("Light Snow Shower"),
  N_("Snow Shower / Wind"),
  N_("Snow Showers Early"),
  N_("Snow Showers early"),
  N_("Snow Showers Late"),
  N_("Snow to Rain"),
  N_("Sunny"),
  N_("Sunny / Wind"),
  N_("T-Showers"),
  N_("T-Storm"),
  N_("T-Storms"),
  N_("T-Storms / Wind"),
  N_("T-Storms Early"),
  N_("Thunder"),
  N_("Thunder in the Vicinity"),
  N_("Thunder in the Vincinity"),
  N_("Variable Cloudiness"),
  N_("Variable Clouds"),
  N_("Windy/Rain"),
  N_("Windy/Snow"),
  N_("Wintry Mix"),
  NULL
};



static const gchar *bard_strings[] = {
  N_("rising"),
  N_("steady"),
  N_("falling"),
  NULL
};



static const gchar *risk_strings[] = {
  N_("Low"),
  N_("Moderate"),
  N_("High"),
  N_("Very High"),
  N_("Extreme"),
  NULL
};



static const gchar *wdirs[] = {
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



static const gchar *
translate_str (const gchar **loc_strings,
               const gchar  *str)
{
  gint  loc_string_len, str_len;
  guint i;

  if (str == NULL)
    return "?";

  str_len = strlen (str);

  if (str_len < 1)
    return "?";

  for (i = 0; loc_strings[i] != NULL; i++)
    {
      loc_string_len = strlen (loc_strings[i]);

      if (str_len != loc_string_len)
        continue;

      if (str[0] != loc_strings[i][0])
        continue;

      if (!g_ascii_strncasecmp (loc_strings[i], str, str_len))
        return _(loc_strings[i]);
    }

  return str;
}



const gchar *
translate_bard (const gchar *bard)
{
  return translate_str (bard_strings, bard);
}



const gchar *
translate_risk (const gchar *risk)
{
  return translate_str (risk_strings, risk);
}



const gchar *
translate_desc (const gchar *desc)
{
  return translate_str (desc_strings, desc);
}



/* used by translate_lsup and translate_time */
static void
_fill_time (struct tm   *time_tm,
            const gchar *hour,
            const gchar *minute,
            const gchar *am)
{
  time_tm->tm_hour = atoi (hour);

  if (am[0] == 'P' && time_tm->tm_hour != 12)        /* PM or AM */
    time_tm->tm_hour += 12;

  time_tm->tm_min = atoi (minute);
  time_tm->tm_sec = 0;
  time_tm->tm_isdst = -1;
}



gchar *
translate_lsup (const gchar *lsup)
{
  gchar      *hdate;
  struct tm   time_tm;
  gint        size = 0, i = 0;
  gchar      **lsup_split;
  int          len;

  if (lsup == NULL || strlen (lsup) == 0)
    return NULL;

  /* 10/17/04 5:55 PM Local Time */
  if ((lsup_split = g_strsplit_set (lsup, " /:", 8)) == NULL)
    return NULL;

  while (lsup_split[i++])
    size++;

  if (size != 8)
    {
      g_strfreev (lsup_split);
      return NULL;
    }

  time_tm.tm_mon = atoi (lsup_split[0]) - 1;
  time_tm.tm_mday = atoi (lsup_split[1]);
  time_tm.tm_year = atoi (lsup_split[2]) + 100;

  _fill_time (&time_tm, lsup_split[3], lsup_split[4], lsup_split[5]);

  g_strfreev (lsup_split);

  if (mktime (&time_tm) != -1)
    {
      hdate = g_malloc (HDATE_N);

      len = strftime (hdate, HDATE_N, _("%x at %X Local Time"), &time_tm);
      hdate[len] = 0;
      if (!g_utf8_validate(hdate, -1, NULL)) {
         gchar *utf8 = g_locale_to_utf8(hdate, -1, NULL, NULL, NULL);
	 g_free(hdate);
	 hdate = utf8;
      }
      return hdate;
    }
  else
    return NULL;
}



gchar *
translate_day (const gchar *day)
{
  gint         wday = -1;
  struct tm    time_tm;
  guint        i;
  const gchar *days[] = {"su", "mo", "tu", "we", "th", "fr", "sa", NULL};
  gchar       *day_loc;
  int          len;

  if (day == NULL || strlen (day) < 2)
    return NULL;

  for (i = 0; days[i] != NULL; i++)
    if (!g_ascii_strncasecmp (day, days[i], 2))
      wday = i;

  if (wday == -1)
    return NULL;
  else
    {
      time_tm.tm_wday = wday;

      day_loc = g_malloc (DAY_LOC_N);

      len = strftime (day_loc, DAY_LOC_N, "%A", &time_tm);
      day_loc[len] = 0;
      if (!g_utf8_validate(day_loc, -1, NULL)) {
         gchar *utf8 = g_locale_to_utf8(day_loc, -1, NULL, NULL, NULL);
	 g_free(day_loc);
	 day_loc = utf8;
      }

      return day_loc;
    }
}



/* NNW  VAR */
gchar *
translate_wind_direction (const gchar *wdir)
{
  gint   wdir_len;
  guint  i;
  gchar *wdir_loc, *tmp;
  gchar  wdir_i[2];

  if (wdir == NULL || (wdir_len = strlen (wdir)) < 1)
    return NULL;

  if (strchr (wdir, '/'))        /* N/A */
    return NULL;

  /*
   * If the direction code can be translated, then translated the
   * whole code so that it can be correctly translated in CJK (and
   * possibly Finnish).  If not, use the old behaviour where
   * individual direction codes are successively translated.
   */
  if (g_ascii_strcasecmp (wdir, _(wdir)) != 0)
    wdir_loc = g_strdup (_(wdir));
  else
    {
      wdir_loc = g_strdup ("");
      for (i = 0; i < strlen (wdir); i++)
        {
          wdir_i[0] = wdir[i];
          wdir_i[1] = '\0';

          tmp = g_strdup_printf ("%s%s", wdir_loc,
                                 translate_str (wdirs, wdir_i));
          g_free (wdir_loc);
          wdir_loc = tmp;
        }

    }

  return wdir_loc;
}



/* calm or a number */
gchar *
translate_wind_speed (const gchar *wspeed,
                      units        unit)
{
  gchar *wspeed_loc;

  if (g_ascii_strcasecmp (wspeed, "calm") == 0)
    wspeed_loc = g_strdup (_("calm"));
  else if (g_ascii_strcasecmp (wspeed, "N/A") == 0)
    wspeed_loc = g_strdup (_("N/A"));
  else
    wspeed_loc =
      g_strdup_printf ("%s %s", wspeed, get_unit (/* FIXME */NULL, unit, WIND_SPEED));

  return wspeed_loc;
}



gchar *
translate_time (const gchar *timestr)
{
  gchar     **time_split, *time_loc;
  gint        i = 0, size = 0;
  struct tm   time_tm;
  int          len;

  if (strlen (timestr) == 0)
    return NULL;

  time_split = g_strsplit_set (timestr, ": ", 3);

  while (time_split[i++])
    size++;

  if (size != 3)
    return NULL;

  time_loc = g_malloc (TIME_LOC_N);

  _fill_time (&time_tm, time_split[0], time_split[1], time_split[2]);
  g_strfreev (time_split);

  len = strftime (time_loc, TIME_LOC_N, "%X", &time_tm);
  time_loc[len] = 0;
  if (!g_utf8_validate(time_loc, -1, NULL)) {
     gchar *utf8 = g_locale_to_utf8(time_loc, -1, NULL, NULL, NULL);
     g_free(time_loc);
     time_loc = utf8;
  }

  return time_loc;
}
