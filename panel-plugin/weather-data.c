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

#include <libxfce4util/libxfce4util.h>

#include "weather-parsers.h"
#include "weather-data.h"
#include "weather.h"

#define DATAS_CC    0x0100
#define DATAS_LOC   0x0200
#define DATAS_DAYF  0x0300
#define DATAS_LNK   0x0400

#define EMPTY_STRING "99"
#define CHK_NULL(str) (str ? str : EMPTY_STRING);

static const gchar *
get_data_uv (xml_uv   *data,
             datas_uv  type)
{
  gchar *str = NULL;

  if (!data)
    {
      DBG ("get_data_bar: xml-uv not present");
    }
  else
    {
      switch (type)
        {
        case _UV_INDEX:
          str = data->i;
          break;
        case _UV_TRANS:
          str = data->t;
          break;
        }
    }

  return CHK_NULL (str);
}



static const gchar *
get_data_bar (xml_bar   *data,
              datas_bar  type)
{
  gchar *str = NULL;

  if (!data)
    {
      DBG ("get_data_bar: xml-wind not present");
    }
  else
    {
      switch (type)
        {
        case _BAR_R:
          str = data->r;
          break;
        case _BAR_D:
          str = data->d;
          break;
        }
    }

  return CHK_NULL (str);
}



static const gchar *
get_data_wind (xml_wind   *data,
               datas_wind  type)
{
  gchar *str = NULL;

  if (!data)
    {
      DBG ("get_data_wind: xml-wind not present");
    }
  else
    {
      switch (type)
        {
        case _WIND_SPEED:
          str = data->s;
          break;
        case _WIND_GUST:
          str = data->gust;
          break;
        case _WIND_DIRECTION:
          str = data->t;
          break;
        case _WIND_TRANS:
          str = data->d;
          break;
        }
    }

  return CHK_NULL (str);
}



/* -- This is not the same as the previous functions */
static const gchar *
get_data_cc (xml_cc *data,
             datas   type)
{
  gchar *str = NULL;

  if (!data)
    {
      DBG ("get_data_cc: xml-cc not present");
    }
  else
    {
      switch (type)
        {
        case LSUP:
          str = data->lsup;
          break;
        case OBST:
          str = data->obst;
          break;
        case FLIK:
          str = data->flik;
          break;
        case TRANS:
          str = data->t;
          break;
        case TEMP:
          str = data->tmp;
          break;
        case HMID:
          str = data->hmid;
          break;
        case VIS:
          str = data->vis;
          break;
        case UV_INDEX:
          return get_data_uv (data->uv, _UV_INDEX);
        case UV_TRANS:
          return get_data_uv (data->uv, _UV_TRANS);
        case WIND_SPEED:
          return get_data_wind (data->wind, _WIND_SPEED);
        case WIND_GUST:
          return get_data_wind (data->wind, _WIND_GUST);
        case WIND_DIRECTION:
          return get_data_wind (data->wind, _WIND_DIRECTION);
        case WIND_TRANS:
          return get_data_wind (data->wind, _WIND_TRANS);
        case BAR_R:
          return get_data_bar (data->bar, _BAR_R);
        case BAR_D:
          return get_data_bar (data->bar, _BAR_D);
        case DEWP:
          str = data->dewp;
          break;
        case WICON:
          str = data->icon;
          break;
        }
    }

  return CHK_NULL (str);
}



static const gchar *
get_data_loc (xml_loc   *data,
              datas_loc  type)
{
  gchar *str = NULL;

  if (!data)
    {
      DBG ("get_data_loc: xml-loc not present");
    }
  else
    {
      switch (type)
        {
        case DNAM:
          str = data->dnam;
          break;
        case SUNR:
          str = data->sunr;
          break;
        case SUNS:
          str = data->suns;
          break;
        }
    }

  return CHK_NULL (str);
}

static const gchar *
get_data_lnk (xml_lnk   *data,
              lnks       type)
{
  gchar *str = NULL;

  if (!data)
    {
      DBG ("get_data_lnk: xml-lnk not present");
    }
  else
    {
      switch (type)
        {
        case LNK1:
          str = data->lnk[0];
          break;
        case LNK2:
          str = data->lnk[1];
          break;
        case LNK3:
          str = data->lnk[2];
          break;
        case LNK4:
          str = data->lnk[3];
          break;
        case LNK1_TXT:
          str = data->lnk_txt[0];
          break;
        case LNK2_TXT:
          str = data->lnk_txt[1];
          break;
        case LNK3_TXT:
          str = data->lnk_txt[2];
          break;
        case LNK4_TXT:
          str = data->lnk_txt[3];
          break;
        }
    }

  return CHK_NULL (str);
}



const gchar *
get_data (xml_weather *data,
         datas         type)
{
  const gchar *str = NULL;

  if (data)
    {

      switch (type & 0xFF00)
        {
        case DATAS_CC:
          str = get_data_cc (data->cc, type);
          break;
        case DATAS_LOC:
          str = get_data_loc (data->loc, type);
          break;
        case DATAS_LNK:
          str = get_data_lnk (data->lnk, type);
          break;
        default:
          str = EMPTY_STRING;
        }
    }

  return CHK_NULL (str);
}



static const gchar *
get_data_part (xml_part *data,
               forecast  type)
{
  const gchar *str = NULL;

  DBG ("now here %s", data->ppcp);

  if (data)
    {
      switch (type & 0x000F)
        {
        case F_ICON:
          str = data->icon;
          break;
        case F_TRANS:
          str = data->t;
          break;
        case F_PPCP:
          str = data->ppcp;
          break;
        case F_W_SPEED:
          str = get_data_wind (data->wind, _WIND_SPEED);
          break;
        case F_W_DIRECTION:
          str = get_data_wind (data->wind, _WIND_DIRECTION);
          break;
        }
    }

  return CHK_NULL (str);
}



const gchar *
get_data_f (xml_dayf *data,
            forecast  type)
{
  const gchar *str = NULL;

  if (data)
    {
      switch (type & 0x0F00)
        {
        case ITEMS:
          switch (type)
            {
            case WDAY:
              str = data->day;
              break;
            case TEMP_MIN:
              str = data->low;
              break;
            case TEMP_MAX:
              str = data->hi;
              break;
	    default:
	      break;
            }
          break;
        case NPART:
          str = get_data_part (data->part[1], type);
          break;
        case DPART:
          str = get_data_part (data->part[0], type);
          break;
        }
    }

  return CHK_NULL (str);
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

  return str;
}
