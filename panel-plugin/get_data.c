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
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxml/parser.h>

#include "parsers.h"
#include "get_data.h"
#include "plugin.h"

#define DATAS_CC    0x0100
#define DATAS_LOC   0x0200
#define DATAS_DAYF  0x0300
#define KILL_RING_S 5

#define EMPTY_STRING g_strdup("-")
#define CHK_NULL(str) str ? g_strdup(str) : EMPTY_STRING;
gchar *kill_ring[KILL_RING_S] = {NULL, };

#define debug_print printf
gchar *
copy_buffer (gchar *str)
{
        static int p = 0;
        gchar *s;
        
        if (!str)
        {
                //DBG ("copy_buffer: received NULL pointer");
                return EMPTY_STRING;
        }

        if (p >= KILL_RING_S)
                p = 0;

        if (kill_ring[p])
                g_free(kill_ring[p]);

        s = g_strdup(str);

        kill_ring[p++] = s;

        return s;
}

void
free_get_data_buffer (void)
{
        int i;

        for (i = 0; i < KILL_RING_S; i++)
        {
                if (kill_ring[i])
                        g_free(kill_ring[i]);
        }
}

static gchar *
get_data_uv (xml_uv   *data,
             datas_uv  type)
{
        gchar *str = NULL;

        if (!data)
        {
                //DBG ("get_data_bar: xml-uv not present");
                return EMPTY_STRING;
        }

        switch(type)
        {
                case _UV_INDEX: str = data->i; break;
                case _UV_TRANS: str = data->t; break;
        }

        return CHK_NULL(str);
}
 

static gchar *
get_data_bar (xml_bar   *data,
              datas_bar  type)
{
        gchar *str = NULL;

        if (!data)
        {
                //DBG ("get_data_bar: xml-wind not present");
                return EMPTY_STRING;
        }

        switch(type)
        {
                case _BAR_R: str = data->r; break;
                case _BAR_D: str = data->d; break;
        }

        return CHK_NULL(str);
}

static gchar *
get_data_wind (xml_wind   *data,
               datas_wind  type)
{
        gchar *str = NULL;

        if (!data)
        {
                //DBG ("get_data_wind: xml-wind not present");
                return EMPTY_STRING;
        }

       //DBG ("starting");

        switch(type)
        {
                case _WIND_SPEED: str = data->s; break;
                case _WIND_GUST: str = data->gust; break;
                case _WIND_DIRECTION: str = data->t; break;
                case _WIND_TRANS: str = data->d; break;
        }

       //DBG ("print %p", data->d);

       //DBG ("%s", str);

        return CHK_NULL(str);
}

/* -- This is not the same as the previous functions */
static gchar *
get_data_cc (xml_cc *data,
             datas   type)
{ 
        gchar *str = NULL;
        
        if (!data)
        {
                //DBG ("get_data_cc: xml-cc not present");
                return EMPTY_STRING;
        }

        switch(type)
        {
                case LSUP: str = data->lsup; break;
                case OBST: str = data->obst; break;
                case FLIK: str = data->flik; break;
                case TRANS:    str = data->t; break;
                case TEMP:  str = data->tmp; break;
                case HMID: str = data->hmid; break;
                case VIS:  str = data->vis; break;
                case UV_INDEX:   return get_data_uv(data->uv, _UV_INDEX);
                case UV_TRANS:   return get_data_uv(data->uv, _UV_TRANS);
                case WIND_SPEED: return get_data_wind(data->wind, _WIND_SPEED);
                case WIND_GUST: return get_data_wind(data->wind, _WIND_GUST);
                case WIND_DIRECTION: return get_data_wind(data->wind, _WIND_DIRECTION);
                case WIND_TRANS: return get_data_wind(data->wind, _WIND_TRANS);
                case BAR_R:  return get_data_bar(data->bar, _BAR_R);
                case BAR_D: return get_data_bar(data->bar, _BAR_D);
                case DEWP: str = data->dewp; break;
                case WICON: str = data->icon; break;
        }

        return CHK_NULL(str);
}

static gchar *
get_data_loc (xml_loc   *data,
              datas_loc  type)
{ 
        gchar *str = NULL;
        
        if (!data)
        {
                //DBG ("get_data_loc: xml-loc not present");
                return EMPTY_STRING;
        }

        switch(type)
        {
                case DNAM: str = data->dnam; break;
                case SUNR: str = data->sunr; break;
                case SUNS: str = data->suns; break;
        }

        return CHK_NULL(str);
}
 

const gchar *
get_data (xml_weather *data,
          datas        type)
{
        gchar *str = NULL;
        gchar *p;

        if (!data)
                str = EMPTY_STRING;
        else 
        {
        
                switch (type & 0xFF00)
                {
                        case DATAS_CC: str = get_data_cc(data->cc, type); break;
                        case DATAS_LOC: str = get_data_loc(data->loc, type); break;
                        default: str = EMPTY_STRING;
                }
        }

        p = copy_buffer(str);
        g_free(str);

        return p;
}

static gchar *
get_data_part (xml_part *data,
               forecast  type)
{
       gchar *str = NULL;

       //DBG ("now here %s", data->ppcp);

       if (!data)
               return EMPTY_STRING;

        switch (type & 0x000F)
        {
                case F_ICON: str = data->icon; break;
                case F_TRANS: str = data->t; break;
                case F_PPCP: str = data->ppcp; break;
                case F_W_SPEED: str = get_data_wind(data->wind, _WIND_SPEED); break;
                case F_W_DIRECTION: str = get_data_wind(data->wind, _WIND_DIRECTION); break;
        }

        return str;
}

const gchar *
get_data_f (xml_dayf *data,
            forecast  type)
{
        gchar *p, *str = NULL;

        if (data)
        {
                switch (type & 0x0F00)
                {
                        case ITEMS:
                                switch(type)
                                {
                                        case WDAY: str = data->day; break;
                                        case TEMP_MIN: str = data->low; break;
                                        case TEMP_MAX: str = data->hi; break;
                                        default: str = g_strdup("-"); break;
                                }
                                break;
                        case NPART:
                                str = get_data_part(data->part[1], type);
                                break;
                        case DPART:
                                str = get_data_part(data->part[0], type);
                                break; 
                }
        }

        if (!str)
                str = "-";
        

        p = copy_buffer(str);
       //DBG ("value: %s", p);
        return p;
}

const gchar *
get_unit (units unit,
          datas type)
{
        gchar *str;
        
        switch (type & 0x00F0)
        {
                case 0x0020: 
                        str = (unit == METRIC ? "\302\260C" : "\302\260F");
                        break;
                case 0x0030:
                        str = "%";
                        break;
                case 0x0040:
                        str = (unit == METRIC ? _("km/h") : _("mph"));
                        break;
                case 0x0050: 
                        str = (unit == METRIC ? _("hPa") : _("in"));
                        break;
                case 0x0060:
                        str = (unit == METRIC ? _("km") : _("mi"));
                        break;
                default:
                        str = "";
        }

        return copy_buffer (str);
}

