/*
** tea_list.c
** List handling
*/

#define tea_list_c
#define TEA_CORE

#include "tea_list.h"
#include "tea_gc.h"

GClist* tea_list_new(tea_State* T)
{
    GClist* list = tea_obj_new(T, GClist, TEA_TLIST);
    list->items = NULL;
    list->count = 0;
    list->size = 0;
    return list;
}

void tea_list_add(tea_State* T, GClist* list, TValue* value)
{
    if(list->size < list->count + 1)
    {
        list->items = tea_mem_growvec(T, TValue, list->items, list->size, INT_MAX);
    }
    copyTV(T, list->items + list->count, value);
    list->count++;
}