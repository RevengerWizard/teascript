/*
** tea_lib.c
** Library function support
*/

#include <errno.h>

#define tea_lib_c
#define TEA_CORE

#include "tea_lib.h"
#include "tea_err.h"

/* -- Type checks --------------------------------------------------------- */

TValue* tea_lib_checkany(tea_State* T, int index)
{
    TValue* o = T->base + index;
    if(o >= T->top)
        tea_err_arg(T, index, TEA_ERR_NOVAL);
    return o;
}

tea_Number tea_lib_checknumber(tea_State* T, int index)
{
    TValue* o = T->base + index;
    if(!(o < T->top && tvisnum(o)))
        tea_err_argt(T, index, TEA_TYPE_NUMBER);
    return numV(o);
}

int32_t tea_lib_checkint(tea_State* T, int index)
{
    TValue* o = T->base + index;
    if(!(o < T->top && tvisnum(o)))
        tea_err_argt(T, index, TEA_TYPE_NUMBER);
    return (int32_t)numV(o);
}

int32_t tea_lib_optint(tea_State* T, int index, int32_t def)
{
    TValue* o = T->base + index;
    return (o < T->top) ? tea_lib_checkint(T, index) : def;
}

GCstr* tea_lib_checkstr(tea_State* T, int index)
{
    TValue* o = T->base + index;
    if(!(o < T->top && tvisstr(o)))
        tea_err_argt(T, index, TEA_TYPE_STRING);
    return strV(o);
}

GCstr* tea_lib_optstr(tea_State* T, int index)
{
    TValue* o = T->base + index;
    return (o < T->top) ? tea_lib_checkstr(T, index) : NULL;
}

GClist* tea_lib_checklist(tea_State* T, int index)
{
    TValue* o = T->base + index;
    if(!(o < T->top && tvislist(o)))
        tea_err_argt(T, index, TEA_TYPE_LIST);
    return listV(o);
}

GCmap* tea_lib_checkmap(tea_State* T, int index)
{
    TValue* o = T->base + index;
    if(!(o < T->top && tvismap(o)))
        tea_err_argt(T, index, TEA_TYPE_MAP);
    return mapV(o);
}

GCrange* tea_lib_checkrange(tea_State* T, int index)
{
    TValue* o = T->base + index;
    if(!(o < T->top && tvisrange(o)))
        tea_err_argt(T, index, TEA_TYPE_RANGE);
    return rangeV(o);
}

int tea_lib_checkopt(tea_State* T, int index, int def, const char* lst)
{
    GCstr* s = def >= 0 ? tea_lib_optstr(T, index) : tea_lib_checkstr(T, index);
    if(s)
    {
        const char* opt = str_data(s);
        size_t len = s->len;
        int i;
        for(i = 0; *(const uint8_t*)lst; i++)
        {
            if(*(const uint8_t*)lst == len && memcmp(opt, lst + 1, len) == 0)
                return i;
            lst += 1 + *(const uint8_t*)lst;
        }
        tea_error(T, "Invalid option " TEA_QS, opt);
    }
    return def;
}

int32_t tea_lib_checkintrange(tea_State* T, int index, int32_t a, int32_t b)
{
    TValue* o = T->base + index;
    if(o < T->top)
    {
        if(tvisnum(o))
        {
            int32_t i = (int32_t)numV(o);
            if(i >= a && i <= b) return i;
        }
        else
        {
            goto badtype;
        }
        tea_err_arg(T, index, TEA_ERR_INTRANGE);
    }
badtype:
    tea_err_argt(T, index, TEA_TYPE_NUMBER);
    return 0;
}

void tea_lib_fileresult(tea_State* T, const char* fname)
{
    int en = errno; /* Teascript API calls may change this value */
    if(fname)
        tea_error(T, "%s: %s", fname, strerror(en));
    else
        tea_error(T, "%s", strerror(en));
}