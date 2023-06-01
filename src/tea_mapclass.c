/*
** tea_mapclass.c
** Teascript map class
*/

#include "tea_vm.h"
#include "tea_core.h"

static void map_len(TeaState* T)
{
    tea_push_number(T, tea_len(T, 0));
}

static void map_keys(TeaState* T)
{
    TeaObjectMap* map = AS_MAP(T->base[0]);

    tea_new_list(T);

    TeaObjectList* list = AS_LIST(T->base[1]);
    for(int i = 0; i < map->capacity; i++)
    {
        if(map->items[i].empty) continue;
        tea_write_value_array(T, &list->items, map->items[i].key);
    }
}

static void map_values(TeaState* T)
{
    TeaObjectMap* map = AS_MAP(T->base[0]);

    tea_new_list(T);

    TeaObjectList* list = AS_LIST(T->base[1]);
    for(int i = 0; i < map->capacity; i++)
    {
        if(map->items[i].empty) continue;
        tea_write_value_array(T, &list->items, map->items[i].value);
    }
}

static void map_clear(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    TeaObjectMap* map = AS_MAP(T->base[0]);
    map->items = NULL;
    map->capacity = 0;
    map->count = 0;
}

static void map_contains(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaObjectMap* map = AS_MAP(T->base[0]);

    if(!teaO_is_valid_key(T->base[1]))
    {
        tea_error(T, "Map key isn't hashable");
    }
    
    TeaValue _;
    tea_push_bool(T, teaO_map_get(map, T->base[1], &_));
}

static void map_delete(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaObjectMap* map = AS_MAP(T->base[0]);
    TeaValue _;
    if(!teaO_is_valid_key(T->base[1]))
    {
        tea_error(T, "Map key isn't hashable");
    }
    else if(!teaO_map_get(map, T->base[1], &_))
    {
        tea_error(T, "No such key in the map");
    }

    teaO_map_delete(map, T->base[1]);
}

static void map_copy(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    TeaObjectMap* map = AS_MAP(T->base[0]);

    tea_new_map(T);

    TeaObjectMap* new = AS_MAP(T->base[1]);
    for(int i = 0; i < map->capacity; i++)
    {
        if(map->items[i].empty) continue;
        teaO_map_set(T, new, map->items[i].key, map->items[i].value);
    }
}

static void map_iterate(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaObjectMap* map = AS_MAP(T->base[0]);
    if(map->count == 0)
    {
        tea_push_null(T);
        return;
    }

    // If we're starting the iteration, start at the first used entry
    int index = 0;

    // Otherwise, start one past the last entry we stopped at
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

        // Advance the iterator
        index++;
    }

    // Find a used entry, if any
    for(; index < map->capacity; index++)
    {
        if (!map->items[index].empty)
        {
            tea_push_number(T, index);
            return;
        }
    }

    // If we get here, walked all of the entries
    tea_push_null(T);
}

static void map_iteratorvalue(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaObjectMap* map = AS_MAP(T->base[0]);
    int index = tea_check_number(T, 1);

    TeaMapItem* item = &map->items[index];
    if(item->empty)
    {
        tea_error(T, "Invalid map iterator");
    }

    tea_new_list(T);
    teaV_push(T, item->key);
    tea_add_item(T, 2);
    teaV_push(T, item->value);
    tea_add_item(T, 2);
}

static const TeaClass map_class[] = {
    { "len", "property", map_len },
    { "keys", "property", map_keys },
    { "values", "property", map_values },
    { "clear", "method", map_clear },
    { "contains", "method", map_contains },
    { "delete", "method", map_delete },
    { "copy", "method", map_copy },
    { "iterate", "method", map_iterate },
    { "iteratorvalue", "method", map_iteratorvalue },
    { NULL, NULL, NULL }
};

void tea_open_map(TeaState* T)
{
    tea_create_class(T, TEA_MAP_CLASS, map_class);
    T->map_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_MAP_CLASS);
    tea_push_null(T);
}