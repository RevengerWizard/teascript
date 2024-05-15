/*
** tea_lib.c
** Library function support
*/

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
    if(!(o < T->top && tvisnumber(o)))
        tea_err_argt(T, index, TEA_TYPE_NUMBER);
    return numberV(o);
}

GCstr* tea_lib_checkstr(tea_State* T, int index)
{
    TValue* o = T->base + index;
    if(!(o < T->top && tvisstr(o)))
        tea_err_argt(T, index, TEA_TYPE_STRING);
    return strV(o);
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

void tea_lib_fileresult(tea_State* T, const char* fname)
{
    int en = errno; /* Teascript API calls may change this value */
    if(fname)
        tea_error(T, "%s: %s", fname, strerror(en));
    else
        tea_error(T, "%s", strerror(en));
}