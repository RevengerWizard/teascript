/*
** tea_list.c
** List handling
*/

#include <math.h>

#define tea_list_c
#define TEA_CORE

#include "tea_list.h"
#include "tea_gc.h"
#include "tea_err.h"

/* Create a new list, with a given optional size */
GClist* tea_list_new(tea_State* T, size_t n)
{
    TValue* items = NULL;
    if(n > 0)
    {
        if(TEA_UNLIKELY(n > TEA_MAX_MEM32))
            tea_err_msg(T, TEA_ERR_LISTOV);       
        items = tea_mem_newvec(T, TValue, n);
    }
    GClist* list = tea_mem_newobj(T, GClist, TEA_TLIST);
    list->items = items;
    list->len = 0;
    list->size = (uint32_t)n;
    return list;
}

/* Free a list */
void TEA_FASTCALL tea_list_free(tea_State* T, GClist* list)
{
    tea_mem_freevec(T, TValue, list->items, list->size);
    tea_mem_freet(T, list);
}

/* Copy a list */
GClist* tea_list_copy(tea_State* T, GClist* list)
{
    GClist* l = tea_list_new(T, 0);
    for(uint32_t i = 0; i < list->len; i++)
    {
        TValue* o = list_slot(list, i);
        tea_list_add(T, l, o);
    }
    return l;
}

/* Add an item to the end of a list */
void tea_list_add(tea_State* T, GClist* list, cTValue* o)
{
    if(list->size < list->len + 1)
    {
        list->items = tea_mem_growvec(T, TValue, list->items, list->size, TEA_MAX_MEM32);
    }
    copyTV(T, list_slot(list, list->len), o);
    list->len++;
}

/* Add a number to the end of a list */
void tea_list_addn(tea_State* T, GClist* list, double n)
{
    if(list->size < list->len + 1)
    {
        list->items = tea_mem_growvec(T, TValue, list->items, list->size, TEA_MAX_MEM32);
    }
    setnumV(list_slot(list, list->len), n);
    list->len++;
}

/* Insert an item into a list */
void tea_list_insert(tea_State* T, GClist* list, cTValue* o, int32_t idx)
{
    if(list->size < list->len + 1)
    {
        list->items = tea_mem_growvec(T, TValue, list->items, list->size, TEA_MAX_MEM32);
    }
    list->len++;
    for(uint32_t i = list->len - 1; i > idx; i--)
    {
        copyTV(T, list_slot(list, i), list_slot(list, i - 1));
    }
    copyTV(T, list_slot(list, idx), o);
}

/* Delete an item from a list */
void tea_list_delete(tea_State* T, GClist* list, int32_t idx)
{
    for(uint32_t i = idx; i < list->len - 1; i++)
    {
        copyTV(T, list_slot(list, i), list_slot(list, i + 1));
    }
    list->len--;
}

/* Slice a list */
GClist* tea_list_slice(tea_State* T, GClist* list, GCrange* range)
{
    GClist* new_list = tea_list_new(T, 0);
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

    for(int32_t i = start; 
        step > 0 ? (i < end) : (i > end); 
        i += step)
    {
        tea_list_add(T, new_list, list_slot(list, i));
    }

    T->top--;   /* Pop the pushed list */
    return new_list;
}