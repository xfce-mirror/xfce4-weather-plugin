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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <libxml/parser.h>

#include "parsers.h"

xml_weather *
parse_weather (xmlNode *cur_node)
{
    xml_weather *ret;

    if (!NODE_IS_TYPE (cur_node, "weather")) {
        return NULL;
    }

    if ((ret = g_new0(xml_weather, 1)) == NULL)
        return NULL;

    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE (cur_node, "cc"))
            ret->cc = parse_cc(cur_node);
        else if (NODE_IS_TYPE (cur_node, "loc"))
            ret->loc = parse_loc(cur_node);
        else if (NODE_IS_TYPE (cur_node, "dayf"))
        {
            xmlNode *child_node;
            guint i = 0;

            for (child_node = cur_node->children; child_node; child_node = child_node->next)
            {
                if (NODE_IS_TYPE (child_node, "day"))
                {
                    if (i >= XML_WEATHER_DAYF_N)
                        break;
                
                    ret->dayf[i] = parse_dayf(child_node);

                    i++;
                }
            }
        }
    }

    return ret;
}

xml_loc *
parse_loc (xmlNode *cur_node)
{
    xml_loc *ret;
    
    if ((ret = g_new0(xml_loc, 1)) == NULL)
        return NULL;


    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE (cur_node, "dnam"))
            ret->dnam = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "sunr"))
            ret->sunr = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "suns"))
            ret->suns = DATA(cur_node);
    }

    return ret;
}

static xml_uv *
parse_uv (xmlNode *cur_node)
{
    xml_uv *ret;
    
    if ((ret = g_new0 (xml_uv, 1)) == NULL)
        return NULL;

    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE (cur_node, "i"))
            ret->i = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "t"))
            ret->t = DATA(cur_node);
    }

    return ret;
}

static xml_bar *
parse_bar (xmlNode *cur_node)
{
    xml_bar *ret;
    
    if ((ret = g_new0(xml_bar, 1)) == NULL)
        return NULL;

    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;
        
        if (NODE_IS_TYPE (cur_node, "r"))
            ret->r = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "d"))
            ret->d = DATA(cur_node);
    }

    return ret;
}

static xml_wind *
parse_wind (xmlNode *cur_node)
{
    xml_wind *ret;
    
    if ((ret = g_new0(xml_wind, 1)) == NULL)
        return NULL;
    
    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE (cur_node, "s"))
            ret->s = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "gust"))
            ret->gust = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "d"))
            ret->d = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "t"))
            ret->t = DATA(cur_node);
    }

    return ret;
}

xml_cc *
parse_cc (xmlNode *cur_node)
{
    xml_cc *ret;
    
    if ((ret = g_new0(xml_cc, 1)) == NULL)
        return NULL;
    
    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE (cur_node, "tmp"))
            ret->tmp = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "icon"))
            ret->icon = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "t"))
            ret->t = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "flik"))
            ret->flik = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "bar"))
            ret->bar = parse_bar(cur_node);
        else if (NODE_IS_TYPE (cur_node, "wind"))
            ret->wind = parse_wind(cur_node);
        else if (NODE_IS_TYPE (cur_node, "hmid"))
            ret->hmid = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "vis"))
            ret->vis = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "uv"))
            ret->uv = parse_uv(cur_node);
        else if (NODE_IS_TYPE (cur_node, "dewp"))
            ret->dewp = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "lsup"))
            ret->lsup = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "obst"))
            ret->obst = DATA(cur_node);
    }

    return ret;
}

static xml_part *
parse_part (xmlNode *cur_node)
{
    xml_part *ret;
    
    if ((ret = g_new0(xml_part, 1)) == NULL)
        return NULL;
    
    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE (cur_node, "icon"))
            ret->icon = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "t"))
            ret->t = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "wind"))
            ret->wind = parse_wind(cur_node);
        else if (NODE_IS_TYPE (cur_node, "ppcp"))
            ret->ppcp = DATA(cur_node);
        else if (NODE_IS_TYPE (cur_node, "hmid"))
            ret->hmid = DATA(cur_node);
    }

    return ret;
}



xml_dayf *
parse_dayf (xmlNode *cur_node)
{
    xml_dayf *ret;  
    gchar *value;

    if ((ret = g_new0(xml_dayf, 1)) == NULL)
        return NULL;

    ret->day  = (gchar *) xmlGetProp (cur_node, (const xmlChar *) "t");
    ret->date = (gchar *) xmlGetProp (cur_node, (const xmlChar *) "dt");

    for (cur_node = cur_node->children; cur_node; cur_node = cur_node->next)
    {
        if (cur_node->type != XML_ELEMENT_NODE)
            continue;

        if (NODE_IS_TYPE (cur_node, "hi"))
        {
            ret->hi = DATA(cur_node); 
            g_assert(ret->hi != NULL);
        }
        else if (NODE_IS_TYPE (cur_node, "low"))
        {
            ret->low = DATA(cur_node);
        }
        else if (NODE_IS_TYPE (cur_node, "part"))
        {
            value = (gchar *)  xmlGetProp (cur_node, (const xmlChar *) "p");
            
            if (xmlStrEqual ((const xmlChar *)value, (const xmlChar *)"d"))
                ret->part[0] = parse_part(cur_node);
            else if (xmlStrEqual ((const xmlChar *)value, (const xmlChar *)"n"))
                ret->part[1] = parse_part(cur_node);
            
            g_free(value);
        }
    }

    return ret;
}

#define CHK_FREE(this) if (this)\
              free(this);

static void
xml_uv_free (xml_uv *data)
{
    CHK_FREE (data->i);
    CHK_FREE (data->t);
}

static void
xml_wind_free (xml_wind *data)
{
    CHK_FREE (data->s);
    CHK_FREE (data->gust);
    CHK_FREE (data->d);
    CHK_FREE (data->t);
}

static void
xml_bar_free (xml_bar *data)
{
    CHK_FREE (data->r);
    CHK_FREE (data->d);
}

static void
xml_cc_free (xml_cc *data)
{
    CHK_FREE (data->obst);
    CHK_FREE (data->lsup);
    CHK_FREE (data->flik);
    CHK_FREE (data->t);
    CHK_FREE (data->icon);
    CHK_FREE (data->tmp);
    CHK_FREE (data->hmid);
    CHK_FREE (data->vis);
    CHK_FREE (data->dewp);

    if (data->uv)
        xml_uv_free(data->uv);

    if (data->wind)
        xml_wind_free(data->wind);

    if (data->bar)
        xml_bar_free(data->bar);
}

static void
xml_loc_free (xml_loc *data)
{
    CHK_FREE (data->dnam);
    CHK_FREE (data->sunr);
    CHK_FREE (data->suns);
}

static void
xml_part_free (xml_part *data)
{
    if (!data)
        return;

    CHK_FREE (data->icon);
    CHK_FREE (data->t);
    CHK_FREE (data->ppcp);
    CHK_FREE (data->hmid);

    if (data->wind)
        xml_wind_free(data->wind);
}

static void
xml_dayf_free (xml_dayf *data)
{
    if (!data)
        return;

    CHK_FREE (data->day);
    CHK_FREE (data->date);
    CHK_FREE (data->hi);
    CHK_FREE (data->low);

    if (data->part[0])
        xml_part_free(data->part[0]);

    if (data->part[1])
        xml_part_free(data->part[1]);
}

void
xml_weather_free (xml_weather *data)
{
    if (data->cc)
        xml_cc_free(data->cc);

    if (data->loc)
        xml_loc_free(data->loc);

    if (data->dayf)
    {
        guint i;
        for (i = 0; i < XML_WEATHER_DAYF_N; i++)
        {
            if (!data->dayf[i])
                break;

            xml_dayf_free(data->dayf[i]);
        }

    }

	free(data);
}
