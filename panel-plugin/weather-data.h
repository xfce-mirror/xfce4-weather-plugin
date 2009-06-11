/*  $Id$
 *
 *  Copyright (c) 2003-2007 Xfce Development Team
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

#ifndef __WEATHER_DATA_H__
#define __WEATHER_DATA_H__

G_BEGIN_DECLS

typedef enum
{
  _WIND_SPEED,
  _WIND_GUST,
  _WIND_DIRECTION,
  _WIND_TRANS
}
datas_wind;

typedef enum
{
  _BAR_R,
  _BAR_D
}
datas_bar;

typedef enum
{
  _UV_INDEX,
  _UV_TRANS
}
datas_uv;

typedef enum
{
  /* cc */
  LSUP           = 0x0101,
  OBST           = 0x0102,
  TRANS          = 0x0103,
  UV_INDEX       = 0x0105,
  UV_TRANS       = 0x0106,
  WIND_DIRECTION = 0x0107,
  BAR_D          = 0x0108,
  WIND_TRANS     = 0x0109,
  WICON          = 0x0110,
  FLIK           = 0x0120,
  TEMP           = 0x0121,
  DEWP           = 0x0122,
  HMID           = 0x0130,
  WIND_SPEED     = 0x0140,
  WIND_GUST      = 0x0141,
  BAR_R          = 0x0150,
  VIS            = 0x0160
}
datas;

typedef enum
{
  DNAM = 0x0201,
  SUNR = 0x0202,
  SUNS = 0x0203
}
datas_loc;

typedef enum
{
  ITEMS         = 0x0100,
  WDAY          = 0x0101,
  TEMP_MIN      = 0x0102,
  TEMP_MAX      = 0x0103,
  F_ICON        = 0x0001,
  F_PPCP        = 0x0002,
  F_W_DIRECTION = 0x0003,
  F_W_SPEED     = 0x0004,
  F_TRANS       = 0x0005,
  NPART         = 0x0200,
  ICON_N        = 0x0201,
  PPCP_N        = 0x0202,
  W_DIRECTION_N = 0x0203,
  W_SPEED_N     = 0x0204,
  TRANS_N       = 0x0205,
  DPART         = 0x0300,
  ICON_D        = 0x0301,
  PPCP_D        = 0x0302,
  W_DIRECTION_D = 0x0303,
  W_SPEED_D     = 0x0304,
  TRANS_D       = 0x0305
}
forecast;

typedef enum
{
  LNK1          = 0x0400,
  LNK2          = 0x0401,
  LNK3          = 0x0402,
  LNK4          = 0x0403,
  LNK1_TXT      = 0x0404,
  LNK2_TXT      = 0x0405,
  LNK3_TXT      = 0x0406,
  LNK4_TXT      = 0x0407
}
lnks;

typedef enum
{
  METRIC,
  IMPERIAL
}
units;

const gchar *get_data (xml_weather * data, datas type);

const gchar *get_data_f (xml_dayf *, forecast type);

const gchar *get_unit (units unit, datas type);

void free_get_data_buffer (void);

G_END_DECLS

#endif
