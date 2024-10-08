/*
** tea_udata.c
** Userdata handling
*/

#define tea_udata_c
#define TEA_CORE

#include "tea_udata.h"
#include "tea_tab.h"
#include "tea_gc.h"

GCudata* tea_udata_new(tea_State* T, size_t len)
{
    GCudata* ud = (GCudata*)tea_mem_newgco(T, tea_udata_size(len), TEA_TUDATA);
    ud->udtype = UDTYPE_USERDATA;
    ud->len = len;
    ud->fd = NULL;
    ud->klass = T->object_class;
    tea_tab_init(&ud->attrs);
    return ud;
}

void TEA_FASTCALL tea_udata_free(tea_State* T, GCudata* ud)
{
    if(ud->fd) ud->fd(ud_data(ud));
    tea_tab_free(T, &ud->attrs);
    tea_mem_free(T, ud, tea_udata_size(ud->len));
}