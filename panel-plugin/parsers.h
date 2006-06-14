/* vim: set expandtab ts=8 sw=4: */

/*  This program is free software; you can redistribute it and/or modify
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

#ifndef PARSERS_H
#define PARSERS_H

#include <glib.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>

G_BEGIN_DECLS

#define DATA(node) (gchar *) xmlNodeListGetString(node->doc, node->children, 1)
#define NODE_IS_TYPE(node, type) xmlStrEqual (node->name, (const xmlChar *) type)

#define XML_WEATHER_DAYF_N 5

typedef struct
{
        gchar *dnam;
        gchar *sunr;
        gchar *suns;
}
xml_loc;

typedef struct 
{
        gchar *i;
        gchar *t;
}
xml_uv;

typedef struct
{
        gchar *s;
        gchar *gust;
        gchar *d;
        gchar *t;
}
xml_wind;

typedef struct
{
        gchar *r;
        gchar *d;
}
xml_bar;

typedef struct
{
        gchar    *lsup;
        gchar    *obst;
        gchar    *flik;
        gchar    *t;
        gchar    *icon;
        gchar    *tmp;
        
        gchar    *hmid;
        gchar    *vis;
        
        xml_uv   *uv;
        xml_wind *wind;
        xml_bar  *bar;

        gchar    *dewp;
}
xml_cc;

typedef struct
{
        gchar    *icon;
        gchar    *t;
        gchar    *ppcp;
        gchar    *hmid;

        xml_wind *wind;
}
xml_part;

typedef struct 
{
        gchar    *day;
        gchar    *date;

        gchar    *hi;
        gchar    *low;

        xml_part *part[2];
}
xml_dayf;

typedef struct
{
        xml_loc  *loc;
        xml_cc   *cc;
        xml_dayf *dayf[XML_WEATHER_DAYF_N];
}
xml_weather;


xml_weather *
parse_weather    (xmlNode *cur_node);

xml_loc *
parse_loc        (xmlNode *cur_node);

xml_cc *
parse_cc         (xmlNode *cur_node);

xml_dayf *
parse_dayf       (xmlNode *cur_node);

void
xml_weather_free (xml_weather *data);

G_END_DECLS

#endif
