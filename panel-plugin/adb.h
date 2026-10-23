#ifndef XFCE4_WEATHER_PLUGIN_ADB_H
#define XFCE4_WEATHER_PLUGIN_ADB_H

#include <cairo.h>
#include <glib.h>
#include <math.h>

#include "weather-icon.h"
#include "weather-parsers.h"
#include "weather-data.h"
#include "weather-debug.h"
#include "weather-search.h"

#include "weather.h"

/* minimum displacement to count as "moved", in kilometers; sized to
   the met.no forecast grid resolution (~9-11 km): */
#define MIN_DISPLACEMENT (5.0)

/* distance in kilometers below which the altitude lookup is skipped;
   elevation changes by only a few meters over short distances and the
   met.no pressure correction is insensitive at that scale: */
#define ALTITUDE_DISPLACEMENT (20.0)

/* distance in kilometers below which the timezone lookup is skipped;
   timezone boundaries are hundreds of kilometers apart in populated
   areas, and a wrong timezone shifts every displayed time by hours: */
#define TIMEZONE_DISPLACEMENT (50.0)

/* mean Earth radius in kilometers, used for the spherical
   great-circle approximation in distance_km(): */
#define EARTH_RADIUS_KM (6371.0)

/* degrees to radians conversion factor: */
#define DEG_TO_RAD (G_PI / 180.0)

typedef struct {
    gchar *serial;
    gchar *model;
} device;


device *get_devices(gsize *n_out);


gchar *call_adb(gchar **argv);


gchar *loc_dump(void);


gboolean parse_dump(const gchar *dump, gchar **lat_str, gchar **lon_str);


gboolean get_loc(gchar **lat_str, gchar **lon_str);


gdouble distance_km(gdouble x_lat, gdouble x_lon, gdouble y_lat, gdouble y_lon);

gboolean update_loc(plugin_data *data);


typedef struct {
   void (*cb_show_phones)(GtkMenuItem *menu_item, gpointer user_data);
} adb_t;

extern const adb_t adb;

#endif //XFCE4_WEATHER_PLUGIN_ADB_H
