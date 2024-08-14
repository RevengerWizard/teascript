/*
** tea_list.c
** List handling
*/

#include <math.h>

#define tea_list_c
#define TEA_CORE

#include "tea_list.h"
#include "tea_gc.h"

GClist* tea_list_new(tea_State* T)
{
    GClist* list = tea_mem_newobj(T, GClist, TEA_TLIST);
    list->items = NULL;
    tea_list_clear(list);
    list->size = 0;
    return list;
}

void TEA_FASTCALL tea_list_free(tea_State* T, GClist* list)
{
    tea_mem_freevec(T, TValue, list->items, list->size);
    tea_mem_freet(T, list);
}

GClist* tea_list_copy(tea_State* T, GClist* list)
{
    GClist* l = tea_list_new(T);
    for(int i = 0; i < list->len; i++)
    {
        TValue* o = list_slot(list, i);
        tea_list_add(T, l, o);
    }
    return l;
}

void tea_list_add(tea_State* T, GClist* list, cTValue* o)
{
    if(list->size < list->len + 1)
    {
        list->items = tea_mem_growvec(T, TValue, list->items, list->size, INT_MAX);
    }
    copyTV(T, list_slot(list, list->len), o);
    list->len++;
}

void tea_list_insert(tea_State* T, GClist* list, cTValue* o, int32_t idx)
{
    if(list->size < list->len + 1)
    {
        list->items = tea_mem_growvec(T, TValue, list->items, list->size, INT_MAX);
    }
    list->len++;
    for(int i = list->len - 1; i > idx; i--)
    {
        list->items[i] = list->items[i - 1];
    }
    copyTV(T, list_slot(list, idx), o);
}

void tea_list_delete(tea_State* T, GClist* list, int32_t idx)
{
    for(int i = idx; i < list->len - 1; i++)
    {
        copyTV(T, list_slot(list, i), list_slot(list, i + 1));
    }
    list->len--;
}

GClist* tea_list_slice(tea_State* T, GClist* list, GCrange* range)
{
    GClist* new_list = tea_list_new(T);
    setlistV(T, T->top++, new_list);

    int32_t start = range->start;
    int32_t end;
    int32_t step = range->step;

    if(isinf(range->end))
    {
        end = list->len;
    }
    else
    {
        end = range->end;
        if(end > list->len)
        {
            end = list->len;
        }
        else if(end < 0)
        {
            end = list->len + end;
        }
    }

    if(step > 0)
    {
        for(int i = start; i < end; i += step)
        {
            tea_list_add(T, new_list, list_slot(list, i));
        }
    }
    else if(step < 0)
    {
        for(int i = end + step; i >= start; i += step)
        {
            tea_list_add(T, new_list, list_slot(list, i));
        }
    }

    T->top--;   /* Pop the pushed list */
    return new_list;
}