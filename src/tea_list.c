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
    GClist* list = tea_mem_newobj(T, GClist, TEA_TLIST);
    list->items = NULL;
    list->count = 0;
    list->size = 0;
    return list;
}

GClist* tea_list_copy(tea_State* T, GClist* list)
{
    GClist* l = tea_list_new(T);
    for(int i = 0; i < list->count; i++)
    {
        TValue* o = list_slot(list, i);
        tea_list_add(T, l, o);
    }
    return l;
}

void tea_list_add(tea_State* T, GClist* list, cTValue* o)
{
    if(list->size < list->count + 1)
    {
        list->items = tea_mem_growvec(T, TValue, list->items, list->size, INT_MAX);
    }
    copyTV(T, list_slot(list, list->count), o);
    list->count++;
}

void tea_list_insert(tea_State* T, GClist* list, cTValue* o, int32_t index)
{
    if(list->size < list->count + 1)
    {
        list->items = tea_mem_growvec(T, TValue, list->items, list->size, INT_MAX);
    }
    list->count++;
    for(int i = list->count - 1; i > index; i--)
    {
        list->items[i] = list->items[i - 1];
    }
    copyTV(T, list_slot(list, index), o);
}

void tea_list_delete(tea_State* T, GClist* list, int32_t index)
{
    for(int i = index; i < list->count - 1; i++)
    {
        copyTV(T, list_slot(list, i), list_slot(list, i + 1));
    }
    list->count--;
}