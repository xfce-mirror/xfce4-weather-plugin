#ifndef TRANSLATE_H
#define TRANSLATE_H

#include <gtk/gtk.h>
const gchar *translate_desc(const gchar *);
const gchar *translate_bard(const gchar *);

/* these return a newly alocted string, that should be freed */
gchar *translate_lsup(const gchar *);
gchar *translate_day(const gchar *);
gchar *translate_wind_direction(const gchar *);
gchar *translate_time (const gchar *);

#endif
