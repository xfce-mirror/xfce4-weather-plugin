#ifndef TRANSLATE_H
#define TRANSLATE_H

#include <gtk/gtk.h>
const gchar *translate_desc(const gchar *);
const gchar *translate_bard(const gchar *);
const gchar *translate_risk(const gchar *);

#include "get_data.h"
/* these return a newly alocted string, that should be freed */
gchar *translate_lsup(const gchar *);
gchar *translate_day(const gchar *);
gchar *translate_wind_direction(const gchar *);
gchar *translate_wind_speed(const gchar *, enum units);
gchar *translate_time (const gchar *);
gchar *translate_visibility (const gchar *, enum units);

#endif
