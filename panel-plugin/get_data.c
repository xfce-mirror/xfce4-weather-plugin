#include "get_data.h"
#include "debug_print.h"
#define KILL_RING_S 5

#define EMPTY_STRING g_strdup("-")
#define CHK_NULL(str) str ? g_strdup(str) : EMPTY_STRING;
gchar *kill_ring[KILL_RING_S] = {NULL, };

#define debug_print printf
gchar *copy_buffer(gchar *str)
{
        static int p = 0;
        gchar *s;
        
        if (!str)
        {
                debug_print("copy_buffer: received NULL pointer\n");
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

void free_get_data_buffer(void)
{
        int i;

        for (i = 0; i < KILL_RING_S; i++)
        {
                if (kill_ring[i])
                        g_free(kill_ring[i]);
        }
}

gchar *get_data_uv(struct xml_uv *data, enum datas type)
{
        gchar *str;

        if (!data)
        {
                debug_print("get_data_bar: xml-uv not present\n");
                return EMPTY_STRING;
        }

        switch(type)
        {
                case _UV_INDEX: str = data->i; break;
                case _UV_TRANS: str = data->t; break;
        }

        return CHK_NULL(str);
}
 

gchar *get_data_bar(struct xml_bar *data, enum datas_bar type)
{
        gchar *str;

        if (!data)
        {
                debug_print("get_data_bar: xml-wind not present\n");
                return EMPTY_STRING;
        }

        switch(type)
        {
                case _BAR_R: str = data->r; break;
                case _BAR_D: str = data->d; break;
        }

        return CHK_NULL(str);
}

gchar *get_data_wind(struct xml_wind *data, enum datas_wind type)
{
        gchar *str;

        if (!data)
        {
                debug_print("get_data_wind: xml-wind not present\n");
                return EMPTY_STRING;
        }

       DEBUG_PRINT("starting\n", NULL);

        switch(type)
        {
                case _WIND_SPEED: str = data->s; break;
                case _WIND_GUST: str = data->gust; break;
                case _WIND_DIRECTION: str = data->t; break;
                case _WIND_TRANS: str = data->d; break;
        }

       DEBUG_PRINT("print %p\n", data->d);

       DEBUG_PRINT("%s\n", str);

        return CHK_NULL(str);
}

/* -- This is not the same as the previous functions */
gchar *get_data_cc(struct xml_cc *data, enum datas type)
{ 
        gchar *str;
        
        if (!data)
        {
                debug_print("get_data_cc: xml-cc not present\n");
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

gchar *get_data_loc(struct xml_loc *data, enum datas type)
{ 
        gchar *str;
        
        if (!data)
        {
                debug_print("get_data_loc: xml-loc not present\n");
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
 

const gchar *get_data(struct xml_weather *data, enum datas type)
{
        gchar *str;
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

gchar *get_data_part(struct xml_part *data, enum forecast type)
{
        gchar *str;

       DEBUG_PRINT("now here %s\n", data->ppcp);

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

const gchar *get_data_f(struct xml_dayf *data, enum forecast type)
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
                                }
                                break;
                        case NPART: str = get_data_part(data->part[1], type); break;
                        case DPART: str = get_data_part(data->part[0], type); break; 
                }
        }

        if (!str)
                str = "-";
        

        p = copy_buffer(str);
       DEBUG_PRINT("value: %s\n", p);
        return p;
}

const gchar *get_unit(enum units unit, enum datas type)
{
        gchar *str;
        
        switch (type & 0x00F0)
        {
                case 0x0020: str = (unit == METRIC ? "\302\260C" : "\302\260F"); break;
                case 0x0030: str = "%"; break;
                case 0x0040: str = (unit == METRIC ? "km/h" : "mph"); break;
                case 0x0050: str = (unit == METRIC ? "hPa" : "in"); break;
                default: str = "";
        }

        return copy_buffer(str);
}

