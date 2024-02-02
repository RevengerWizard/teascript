/*
** lib_map.c
** Teascript map class
*/

#define lib_map_c
#define TEA_CORE

#include "tealib.h"

#include "tea_vm.h"
#include "tea_map.h"

static void map_len(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_push_number(T, tea_len(T, 0));
}

static void map_keys(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    GCmap* map = AS_MAP(T->base[0]);

    tea_new_list(T);

    GClist* list = AS_LIST(T->base[1]);
    for(int i = 0; i < map->size; i++)
    {
        if(map->entries[i].empty) continue;
        tea_list_append(T, list, map->entries[i].key);
    }
}

static void map_values(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    GCmap* map = AS_MAP(T->base[0]);

    tea_new_list(T);

    GClist* list = AS_LIST(T->base[1]);
    for(int i = 0; i < map->size; i++)
    {
        if(map->entries[i].empty) continue;
        tea_list_append(T, list, map->entries[i].value);
    }
}

static void map_constructor(tea_State* T)
{
    tea_new_map(T);
}

static void map_get(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 3, "Expected 1 or 2 arguments, got %d", count);

    tea_opt_any(T, 2);

    GCmap* map = AS_MAP(T->base[0]);
    Value key = T->base[1];

    Value value;
    bool b = tea_map_get(map, key, &value);

    if(b)
    {
        tea_vm_push(T, value);
    }
}

static void map_set(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 3, "Expected 1 or 2 arguments, got %d", count);

    tea_opt_any(T, 2);

    GCmap* map = AS_MAP(T->base[0]);
    Value key = T->base[1];
    Value value = T->base[2];

    bool b = tea_map_set(T, map, key, value);

    if(b)
    {
        tea_vm_push(T, value);
    }
}

static void map_update(tea_State* T)
{
    tea_check_type(T, 1, TEA_TYPE_MAP);

    GCmap* map = AS_MAP(T->base[0]);
    GCmap* m = AS_MAP(T->base[1]);

    tea_map_addall(T, m, map);

    tea_push_value(T, 0);
}

static void map_clear(tea_State* T)
{
    GCmap* map = AS_MAP(T->base[0]);
    tea_map_clear(T, map);
}

static void map_contains(tea_State* T)
{
    GCmap* map = AS_MAP(T->base[0]);

    if(!tea_map_hashable(T->base[1]))
    {
        tea_error(T, "Map key isn't hashable");
    }

    Value _;
    tea_push_bool(T, tea_map_get(map, T->base[1], &_));
}

static void map_delete(tea_State* T)
{
    GCmap* map = AS_MAP(T->base[0]);
    Value key = T->base[1];

    Value _;
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

static void map_copy(tea_State* T)
{
    GCmap* map = AS_MAP(T->base[0]);

    tea_new_map(T);

    GCmap* m = AS_MAP(T->base[1]);
    for(int i = 0; i < map->size; i++)
    {
        if(map->entries[i].empty) continue;
        tea_map_set(T, m, map->entries[i].key, map->entries[i].value);
    }
}

static void map_foreach(tea_State* T)
{
    tea_check_function(T, 1);

    GCmap* map = AS_MAP(T->base[0]);

    for(int i = 0; i < map->size; i++)
    {
        if(map->entries[i].empty) continue;

        Value key = map->entries[i].key;
        Value value = map->entries[i].value;

        tea_push_value(T, 1);
        tea_vm_push(T, key);
        tea_vm_push(T, value);
        tea_call(T, 2);
        tea_pop(T, 1);
    }
    tea_set_top(T, 1);
}

static void map_iterate(tea_State* T)
{
    GCmap* map = AS_MAP(T->base[0]);
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

        if(index >= map->size)
        {
            tea_push_null(T);
            return;
        }

        /* Advance the iterator */
        index++;
    }

    /* Find a used entry, if any */
    for(; index < map->size; index++)
    {
        if(!map->entries[index].empty)
        {
            tea_push_number(T, index);
            return;
        }
    }

    /* If we get here, walked all of the entries */
    tea_push_null(T);
}

static void map_iteratorvalue(tea_State* T)
{
    GCmap* map = AS_MAP(T->base[0]);
    int index = tea_check_number(T, 1);

    MapEntry* entry = &map->entries[index];
    if(entry->empty)
    {
        tea_error(T, "Invalid map iterator");
    }

    tea_new_list(T);
    tea_vm_push(T, entry->key);
    tea_add_item(T, 2);
    tea_vm_push(T, entry->value);
    tea_add_item(T, 2);
}

static void map_opadd(tea_State* T)
{
    tea_check_map(T, 0);
    tea_check_map(T, 1);

    GCmap* m1 = AS_MAP(T->base[0]);
    GCmap* m2 = AS_MAP(T->base[1]);

    GCmap* map = tea_map_new(T);
    tea_vm_push(T, OBJECT_VAL(map));

    tea_map_addall(T, m1, map);
    tea_map_addall(T, m2, map);

    tea_pop(T, 3);
    tea_vm_push(T, OBJECT_VAL(map));
}

static const tea_Class map_class[] = {
    { "len", "property", map_len, TEA_VARARGS },
    { "keys", "property", map_keys, TEA_VARARGS },
    { "values", "property", map_values, TEA_VARARGS },
    { "constructor", "method", map_constructor, 1 },
    { "get", "method", map_get, TEA_VARARGS },
    { "set", "method", map_set, TEA_VARARGS },
    { "update", "method", map_update, 2 },
    { "clear", "method", map_clear, 1 },
    { "contains", "method", map_contains, 2 },
    { "delete", "method", map_delete, 2 },
    { "copy", "method", map_copy, 1 },
    { "foreach", "method", map_foreach, 2 },
    { "iterate", "method", map_iterate, 2 },
    { "iteratorvalue", "method", map_iteratorvalue, 2 },
    { "+", "static", map_opadd, 2 },
    { NULL, NULL, NULL }
};

void tea_open_map(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_MAP, map_class);
    T->map_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_CLASS_MAP);
    tea_push_null(T);
}