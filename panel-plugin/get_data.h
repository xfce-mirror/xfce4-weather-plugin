#ifndef GET_DATA_H
#define GET_DATA_H

#include <glib.h>
#include "parsers.h"

#define DATAS_CC    0x0100
#define DATAS_LOC   0x0200
#define DATAS_DAYF  0x0300

enum datas_wind {
        _WIND_SPEED,
        _WIND_GUST,
        _WIND_DIRECTION,
        _WIND_TRANS
};

enum datas_bar {
        _BAR_R,
        _BAR_D
};

enum datas_uv {
        _UV_INDEX,
        _UV_TRANS
};

enum datas {
        /* cc */
        LSUP            = 0x0101,
        OBST            = 0x0102,
        TRANS           = 0x0103,
        UV_INDEX        = 0x0105,
        UV_TRANS        = 0x0106,
        WIND_DIRECTION  = 0x0107,     
        BAR_D           = 0x0108,
        WIND_TRANS      = 0x0109,
        WICON           = 0x0110,
        
        FLIK            = 0x0120, 
        TEMP            = 0x0121,
        DEWP            = 0x0122,
        
        HMID            = 0x0130,
        
        WIND_SPEED      = 0x0140,
        WIND_GUST       = 0x0141,


        BAR_R           = 0x0150,

        VIS             = 0x0160
};


enum datas_loc {
        DNAM            = 0x0201,
        SUNR            = 0x0202,
        SUNS            = 0x0203
};

enum forecast {
        ITEMS           = 0x0100,
        WDAY            = 0x0101,
        TEMP_MIN        = 0x0102,
        TEMP_MAX        = 0x0103,

        F_ICON          = 0x0001,
        F_PPCP          = 0x0002,
        F_W_DIRECTION   = 0x0003,
        F_W_SPEED       = 0x0004,
        F_TRANS         = 0x0005,

        NPART           = 0x0200,
        ICON_N          = 0x0201,
        PPCP_N          = 0x0202,
        W_DIRECTION_N   = 0x0203,
        W_SPEED_N       = 0x0204,
        TRANS_N         = 0x0205,

        DPART           = 0x0300,
        ICON_D          = 0x0301,
        PPCP_D          = 0x0302,
        W_DIRECTION_D   = 0x0303,
        W_SPEED_D       = 0x0304,
        TRANS_D         = 0x0305
};
                

enum units {
        METRIC,
        IMPERIAL
};

const gchar *get_data(struct xml_weather *data, enum datas type);
const gchar *get_data_f(struct xml_dayf * , enum forecast type);
const gchar *get_unit(enum units unit, enum datas type);
void free_get_data_buffer(void);
#endif
