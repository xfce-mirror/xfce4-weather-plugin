#include "parsers.h"
#include "get_data.h"
#include <gtk/gtk.h>
#include "icon.h"

#define BORDER 8
GtkWidget *create_summary_window(struct xml_weather *data, enum units unit);
