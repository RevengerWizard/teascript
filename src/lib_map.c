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
    tea_push_number(T, tea_lib_checkmap(T, 0)->count);
}

static void map_keys(tea_State* T)
{
    GCmap* map = mapV(T->base);

    tea_new_list(T, 0);

    GClist* list = listV(T->base + 1);
    for(uint32_t i = 0; i < map->size; i++)
    {
        if(!tvisnil(&map->entries[i].key))
        {
            tea_list_add(T, list, &map->entries[i].key);
        }
    }
}

static void map_values(tea_State* T)
{
    GCmap* map = mapV(T->base);

    tea_new_list(T, 0);

    GClist* list = listV(T->base + 1);
    for(uint32_t i = 0; i < map->size; i++)
    {
        if(!tvisnil(&map->entries[i].key))
        {
            tea_list_add(T, list, &map->entries[i].val);
        }
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
        if(!tvisnil(&map->entries[i].key))
        {
            TValue* key = &map->entries[i].key;
            TValue* val = &map->entries[i].val;
    
            tea_push_value(T, 1);
            copyTV(T, T->top++, key);
            copyTV(T, T->top++, val);
            tea_call(T, 2);
            tea_pop(T, 1);
        }
    }
    tea_set_top(T, 1);
}

static void map_iternext(tea_State* T)
{
    GCmap* map = mapV(tea_lib_upvalue(T, 0));
    uint32_t idx = (uint32_t)tea_get_number(T, tea_upvalue_index(1));

    if(idx >= map->size || map->count == 0)
    {
        tea_push_nil(T);
        return;
    }

    /* Find a used entry */
    for(; idx < map->size; idx++)
    {
        if(!tvisnil(&map->entries[idx].key))
        {
            tea_new_list(T, 2);
            
            /* Add key to the list */
            copyTV(T, T->top++, &map->entries[idx].key);
            tea_add_item(T, -2);
            /* Add value to the list */
            copyTV(T, T->top++, &map->entries[idx].val);
            tea_add_item(T, -2);

            tea_push_number(T, idx + 1);
            tea_replace(T, tea_upvalue_index(1));
            return;
        }
    }

    tea_push_nil(T);
}

static void map_iter(tea_State* T)
{
    tea_lib_checkmap(T, 0);
    tea_push_number(T, 0);  /* Starting index */
    tea_push_cclosure(T, map_iternext, 2, 0, 0);
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

static const tea_Methods map_reg[] = {
    { "count", "getter", map_count, 1, 0 },
    { "keys", "getter", map_keys, 1, 0 },
    { "values", "getter", map_values, 1, 0 },
    { "new", "method", map_init, 1, 0 },
    { "get", "method", map_get, 2, 1 },
    { "set", "method", map_set, 2, 1 },
    { "update", "method", map_update, 2, 0 },
    { "clear", "method", map_clear, 1, 0 },
    { "contains", "method", map_contains, 2, 0 },
    { "delete", "method", map_delete, 2, 0 },
    { "copy", "method", map_copy, 1, 0 },
    { "foreach", "method", map_foreach, 2, 0 },
    { "iter", "method", map_iter, 1, 0 },
    { "+", "static", map_opadd, 2, 0 },
    { NULL, NULL, NULL }
};

void tea_open_map(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_MAP, map_reg);
    T->gcroot[GCROOT_KLMAP] = obj2gco(classV(T->top - 1));
    tea_set_global(T, TEA_CLASS_MAP);
    tea_push_nil(T);
}