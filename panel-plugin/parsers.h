#ifndef PARSERS_H
#define PARSERS_H

#include <glib.h>
#include <libxml/parser.h>

#define DATA(node) (gchar *) xmlNodeListGetString(node->doc, node->children, 1)
#define NODE_IS_TYPE(node, type) xmlStrEqual (node->name, (const xmlChar *) type)


#define XML_WEATHER_DAYF_N 5

struct xml_weather
{
        struct xml_loc *loc;
        struct xml_cc *cc;
        struct xml_dayf *dayf[XML_WEATHER_DAYF_N];
};
struct xml_loc
{
        gchar *dnam;
        gchar *sunr;
        gchar *suns;
};

struct xml_uv 
{
        gchar *i;
        gchar *t;
};

struct xml_wind
{
        gchar *s;
        gchar *gust;
        gchar *d;
        gchar *t;
};

struct xml_bar
{
        gchar *r;
        gchar *d;
};

struct xml_cc
{
        gchar *lsup;
        gchar *obst;
        gchar *flik;
        gchar *t;
        gchar *icon;
        gchar *tmp;
        
        gchar *hmid;
        gchar *vis;
        
        struct xml_uv *uv;
        struct xml_wind *wind;
        struct xml_bar *bar;

        gchar *dewp;
};

struct xml_part
{
        gchar *icon;
        gchar *t;
        gchar *ppcp;
        gchar *hmid;

        struct xml_wind *wind;
};

struct xml_dayf
{
        gchar *day;
        gchar *date;

        gchar *hi;
        gchar *low;

        struct xml_part *part[2];
};

struct xml_weather *parse_weather(xmlNode *cur_node);
struct xml_loc *parse_loc(xmlNode *cur_node);
struct xml_cc *parse_cc(xmlNode *cur_node);
struct xml_dayf *parse_dayf(xmlNode *cur_node);

void xml_weather_free(struct xml_weather *);
#endif
