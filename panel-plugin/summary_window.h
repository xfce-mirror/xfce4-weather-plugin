#include "parsers.h"
#include "get_data.h"
#include <gtk/gtk.h>
#include "icon.h"


#include <config.h>


#define BORDER 6
GtkWidget *create_summary_window(struct xml_weather *data, enum units unit);
