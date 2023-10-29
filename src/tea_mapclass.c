/*
** tea_mapclass.c
** Teascript map class
*/

#define tea_mapclass_c
#define TEA_CORE

#include "tea_vm.h"
#include "tea_core.h"
#include "tea_map.h"

static void map_len(TeaState* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_push_number(T, tea_len(T, 0));
}

static void map_keys(TeaState* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    TeaOMap* map = AS_MAP(T->base[0]);

    tea_new_list(T);

    TeaOList* list = AS_LIST(T->base[1]);
    for(int i = 0; i < map->capacity; i++)
    {
        if(map->items[i].empty) continue;
        tea_write_value_array(T, &list->items, map->items[i].key);
    }
}

static void map_values(TeaState* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    TeaOMap* map = AS_MAP(T->base[0]);

    tea_new_list(T);

    TeaOList* list = AS_LIST(T->base[1]);
    for(int i = 0; i < map->capacity; i++)
    {
        if(map->items[i].empty) continue;
        tea_write_value_array(T, &list->items, map->items[i].value);
    }
}

static void map_get(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 3, "Expected 1 or 2 arguments, got %d", count);

    tea_opt_any(T, 2);

    TeaOMap* map = AS_MAP(T->base[0]);
    TeaValue key = T->base[1];

    TeaValue value;
    bool b = tea_map_get(map, key, &value);

    if(b)
    {
        tea_vm_push(T, value);
    }
}

static void map_set(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 3, "Expected 1 or 2 arguments, got %d", count);

    tea_opt_any(T, 2);

    TeaOMap* map = AS_MAP(T->base[0]);
    TeaValue key = T->base[1];
    TeaValue value = T->base[2];

    bool b = tea_map_set(T, map, key, value);

    if(b)
    {
        tea_vm_push(T, value);
    }
}

static void map_update(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    tea_check_type(T, 1, TEA_TYPE_MAP);

    TeaOMap* map = AS_MAP(T->base[0]);
    TeaOMap* new = AS_MAP(T->base[1]);

    tea_map_addall(T, new, map);

    tea_push_value(T, 0);
}

static void map_clear(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    TeaOMap* map = AS_MAP(T->base[0]);
    tea_map_clear(T, map);
}

static void map_contains(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaOMap* map = AS_MAP(T->base[0]);

    if(!tea_map_hashable(T->base[1]))
    {
        tea_error(T, "Map key isn't hashable");
    }

    TeaValue _;
    tea_push_bool(T, tea_map_get(map, T->base[1], &_));
}

static void map_delete(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaOMap* map = AS_MAP(T->base[0]);
    TeaValue key = T->base[1];

    TeaValue _;
    if(!tea_map_hashable(key))
    {
        tea_error(T, "Map key isn't hashable");
    }
    
    if(!tea_map_get(map, key, &_))
    {
        tea_error(T, "No such key in the map");
    }

    tea_map_delete(T, map, key);

    tea_push_value(T, 0);
}

static void map_copy(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    TeaOMap* map = AS_MAP(T->base[0]);

    tea_new_map(T);

    TeaOMap* new = AS_MAP(T->base[1]);
    for(int i = 0; i < map->capacity; i++)
    {
        if(map->items[i].empty) continue;
        tea_map_set(T, new, map->items[i].key, map->items[i].value);
    }
}

static void map_foreach(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    tea_check_function(T, 1);

    TeaOMap* map = AS_MAP(T->base[0]);

    for(int i = 0; i < map->capacity; i++)
    {
        if(map->items[i].empty) continue;

        TeaValue key = map->items[i].key;
        TeaValue value = map->items[i].value;

        tea_push_value(T, 1);
        tea_vm_push(T, key);
        tea_vm_push(T, value);
        tea_call(T, 2);
        tea_pop(T, 1);
    }
    tea_set_top(T, 1);
}

static void map_iterate(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaOMap* map = AS_MAP(T->base[0]);
    if(map->count == 0)
    {
        tea_push_null(T);
        return;
    }

    /* If we're starting the iteration, start at the first used entry */
    int index = 0;

    /* Otherwise, start one past the last entry we stopped at */
    if(!tea_is_null(T, 1))
    {
        if(!tea_is_number(T, 1))
        {
            tea_error(T, "Expected a number to iterate");
        }

        index = (uint32_t)tea_get_number(T, 1);
        if(index < 0)
        {
            tea_push_null(T);
            return;
        }

        if(index >= map->capacity)
        {
            tea_push_null(T);
            return;
        }

        /* Advance the iterator */
        index++;
    }

    /* Find a used entry, if any */
    for(; index < map->capacity; index++)
    {
        if (!map->items[index].empty)
        {
            tea_push_number(T, index);
            return;
        }
    }

    /* If we get here, walked all of the entries */
    tea_push_null(T);
}

static void map_iteratorvalue(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaOMap* map = AS_MAP(T->base[0]);
    int index = tea_check_number(T, 1);

    TeaMapItem* item = &map->items[index];
    if(item->empty)
    {
        tea_error(T, "Invalid map iterator");
    }

    tea_new_list(T);
    tea_vm_push(T, item->key);
    tea_add_item(T, 2);
    tea_vm_push(T, item->value);
    tea_add_item(T, 2);
}

static const TeaClass map_class[] = {
    { "len", "property", map_len },
    { "keys", "property", map_keys },
    { "values", "property", map_values },
    { "get", "method", map_get },
    { "set", "method", map_set },
    { "update", "method", map_update },
    { "clear", "method", map_clear },
    { "contains", "method", map_contains },
    { "delete", "method", map_delete },
    { "copy", "method", map_copy },
    { "foreach", "method", map_foreach },
    { "iterate", "method", map_iterate },
    { "iteratorvalue", "method", map_iteratorvalue },
    { NULL, NULL, NULL }
};

void tea_open_map(TeaState* T)
{
    tea_create_class(T, TEA_CLASS_MAP, map_class);
    T->map_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_CLASS_MAP);
    tea_push_null(T);
}