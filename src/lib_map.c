/*
** lib_map.c
** Teascript Map class
*/

#define lib_map_c
#define TEA_CORE

#include "tealib.h"

#include "tea_err.h"
#include "tea_map.h"
#include "tea_list.h"
#include "tea_lib.h"

static void map_count(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_push_number(T, tea_lib_checkmap(T, 0)->count);
}

static void map_keys(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    GCmap* map = mapV(T->base);

    tea_new_list(T, 0);

    GClist* list = listV(T->base + 1);
    for(int i = 0; i < map->size; i++)
    {
        if(tvisnil(&map->entries[i].key))
            continue;
        tea_list_add(T, list, &map->entries[i].key);
    }
}

static void map_values(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    GCmap* map = mapV(T->base);

    tea_new_list(T, 0);

    GClist* list = listV(T->base + 1);
    for(int i = 0; i < map->size; i++)
    {
        if(tvisnil(&map->entries[i].key))
            continue;
        tea_list_add(T, list, &map->entries[i].val);
    }
}

static void map_init(tea_State* T)
{
    tea_new_map(T);
}

static void map_get(tea_State* T)
{
    tea_opt_nil(T, 2);
    GCmap* map = tea_lib_checkmap(T, 0);
    TValue* key = T->base + 1;
    cTValue* o = tea_map_get(map, key);
    if(o)
    {
        copyTV(T, T->top++, o);
    }
}

static void map_set(tea_State* T)
{
    tea_opt_nil(T, 2);
    GCmap* map = tea_lib_checkmap(T, 0);
    TValue* key = tea_lib_checkany(T, 1);
    TValue* value = T->base + 2;
    cTValue* o = tea_map_get(map, key);
    if(!o)
    {
        copyTV(T, tea_map_set(T, map, key), value);
    }
    else
    {
        copyTV(T, T->base + 2, o);
    }
}

static void map_update(tea_State* T)
{
    GCmap* map = tea_lib_checkmap(T, 0);
    GCmap* m = tea_lib_checkmap(T, 1);
    tea_map_merge(T, m, map);
    tea_push_value(T, 0);
}

static void map_clear(tea_State* T)
{
    GCmap* map = tea_lib_checkmap(T, 0);
    tea_map_clear(T, map);
}

static void map_contains(tea_State* T)
{
    GCmap* map = tea_lib_checkmap(T, 0);
    TValue* o = tea_lib_checkany(T, 1);
    tea_push_bool(T, tea_map_get(map, o) != NULL);
}

static void map_delete(tea_State* T)
{
    GCmap* map = tea_lib_checkmap(T, 0);
    TValue* key = tea_lib_checkany(T, 1);
    if(!tea_map_delete(T, map, key))
    {
        tea_err_msg(T, TEA_ERR_MAPKEY);
    }
    tea_push_value(T, 0);
}

static void map_copy(tea_State* T)
{
    GCmap* map = tea_lib_checkmap(T, 0);
    GCmap* newmap = tea_map_copy(T, map);
    setmapV(T, T->top++, newmap);
}

static void map_foreach(tea_State* T)
{
    GCmap* map = tea_lib_checkmap(T, 0);
    tea_check_function(T, 1);

    for(int i = 0; i < map->size; i++)
    {
        if(tvisnil(&map->entries[i].key))
            continue;

        TValue* key = &map->entries[i].key;
        TValue* value = &map->entries[i].val;

        tea_push_value(T, 1);
        copyTV(T, T->top++, key);
        copyTV(T, T->top++, value);
        tea_call(T, 2);
        tea_pop(T, 1);
    }
    tea_set_top(T, 1);
}

static void map_iterate(tea_State* T)
{
    GCmap* map = tea_lib_checkmap(T, 0);
    if(map->count == 0)
    {
        tea_push_nil(T);
        return;
    }

    /* If we're starting the iteration, start at the first used entry */
    int idx = 0;

    /* Otherwise, start one past the last entry we stopped at */
    if(!tea_is_nil(T, 1))
    {
        if(!tea_is_number(T, 1))
        {
            tea_error(T, "Expected a number to iterate");
        }

        idx = (uint32_t)tea_get_number(T, 1);
        if(idx < 0)
        {
            tea_push_nil(T);
            return;
        }

        if(idx >= map->size)
        {
            tea_push_nil(T);
            return;
        }

        /* Advance the iterator */
        idx++;
    }

    /* Find a used entry, if any */
    for(; idx < map->size; idx++)
    {
        if(!tvisnil(&map->entries[idx].key))
        {
            tea_push_number(T, idx);
            return;
        }
    }

    /* If we get here, walked all of the entries */
    tea_push_nil(T);
}

static void map_iteratorvalue(tea_State* T)
{
    GCmap* map = tea_lib_checkmap(T, 0);
    int idx = tea_check_number(T, 1);
    MapEntry* entry = &map->entries[idx];
    if(tvisnil(&entry->key))
    {
        tea_error(T, "Invalid map iterator");
    }
    tea_new_list(T, 2);
    copyTV(T, T->top++, &entry->key);
    tea_add_item(T, 2);
    copyTV(T, T->top++, &entry->val);
    tea_add_item(T, 2);
}

static void map_opadd(tea_State* T)
{
    if(!tvismap(T->base) || !tvismap(T->base + 1))
        tea_err_bioptype(T, T->base, T->base + 1, MM_PLUS);

    GCmap* m1 = tea_lib_checkmap(T, 0);
    GCmap* m2 = tea_lib_checkmap(T, 1);
    GCmap* map = tea_map_new(T);
    setmapV(T, T->top++, map);
    tea_map_merge(T, m1, map);
    tea_map_merge(T, m2, map);
    tea_pop(T, 3);
    setmapV(T, T->top++, map);
}

/* ------------------------------------------------------------------------ */

static const tea_Methods map_class[] = {
    { "count", "property", map_count, TEA_VARG, 0 },
    { "keys", "property", map_keys, TEA_VARG, 0 },
    { "values", "property", map_values, TEA_VARG, 0 },
    { "new", "method", map_init, 1, 0 },
    { "get", "method", map_get, 2, 1 },
    { "set", "method", map_set, 2, 1 },
    { "update", "method", map_update, 2, 0 },
    { "clear", "method", map_clear, 1, 0 },
    { "contains", "method", map_contains, 2, 0 },
    { "delete", "method", map_delete, 2, 0 },
    { "copy", "method", map_copy, 1, 0 },
    { "foreach", "method", map_foreach, 2, 0 },
    { "iterate", "method", map_iterate, 2, 0 },
    { "iteratorvalue", "method", map_iteratorvalue, 2, 0 },
    { "+", "static", map_opadd, 2, 0 },
    { NULL, NULL, NULL }
};

void tea_open_map(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_MAP, map_class);
    T->map_class = classV(T->top - 1);
    tea_set_global(T, TEA_CLASS_MAP);
    tea_push_nil(T);
}