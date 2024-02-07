/*
** tea_map.c
** Map handling
*/

#define tea_map_c
#define TEA_CORE

#include "tea_map.h"
#include "tea_gc.h"

GCmap* tea_map_new(tea_State* T)
{
    GCmap* map = tea_obj_new(T, GCmap, OBJ_MAP);
    map->count = 0;
    map->size = 0;
    map->entries = NULL;

    return map;
}

void tea_map_clear(tea_State* T, GCmap* map)
{
    tea_mem_freevec(T, MapEntry, map->entries, map->size);
    map->entries = NULL;
    map->size = 0;
    map->count = 0;
}

/* 
** From v8's ComputeLongHash() which in turn cites:
** Thomas Wang, Integer Hash Functions.
** http://www.concentric.net/~Ttwang/tech/inthash.htm
*/
static TEA_INLINE uint32_t map_hash(uint64_t hash)
{
    hash = ~hash + (hash << 18);  /* hash = (hash << 18) - hash - 1; */
    hash = hash ^ (hash >> 31);
    hash = hash * 21;  /* hash = (hash + (hash << 2)) + (hash << 4); */
    hash = hash ^ (hash >> 11);
    hash = hash + (hash << 6);
    hash = hash ^ (hash >> 22);
    return (uint32_t)(hash & 0x3fffffff);
}

static TEA_AINLINE uint32_t map_hash_number(double number)
{
    return map_hash(number);
}

static uint32_t map_hash_object(GCobj* object)
{
    switch(object->tt)
    {
        case OBJ_STRING:
            return ((GCstr*)object)->hash;

        default: return 0;
    }
}

static uint32_t map_hash_value(TValue value)
{
    switch(value.tt)
    {
        case VAL_NULL:  return 1;
        case VAL_NUMBER:   return map_hash_number(AS_NUMBER(value));
        case VAL_BOOL:  return 2;
        case VAL_OBJECT:   return map_hash_object(AS_OBJECT(value));
        default:;
    }
    return 0;
}

static MapEntry* map_find_entry(MapEntry* items, int size, TValue key)
{
    uint32_t hash = map_hash_value(key);
    uint32_t index = hash & (size - 1);
    MapEntry* tombstone = NULL;

    while(true)
    {
        MapEntry* item = &items[index];
        if(item->empty)
        {
            if(IS_NULL(item->value))
            {
                /* Empty item */
                return tombstone != NULL ? tombstone : item;
            }
            else
            {
                /* We found a tombstone */
                if(tombstone == NULL)
                    tombstone = item;
            }
        }
        else if(tea_val_rawequal(item->key, key))
        {
            /* We found the key */
            return item;
        }

        index = (index + 1) & (size - 1);
    }
}

bool tea_map_get(GCmap* map, TValue key, TValue* value)
{
    if(map->count == 0)
        return false;

    MapEntry* item = map_find_entry(map->entries, map->size, key);
    if(item->empty)
        return false;

    *value = item->value;

    return true;
}

#define MAP_MAX_LOAD 0.75

static void map_adjust_size(tea_State* T, GCmap* map, int size)
{
    MapEntry* entries = tea_mem_new(T, MapEntry, size);
    for(int i = 0; i < size; i++)
    {
        entries[i].key = NULL_VAL;
        entries[i].value = NULL_VAL;
        entries[i].empty = true;
    }

    map->count = 0;
    for(int i = 0; i < map->size; i++)
    {
        MapEntry* item = &map->entries[i];
        if(item->empty)
            continue;

        MapEntry* dest = map_find_entry(entries, size, item->key);
        dest->key = item->key;
        dest->value = item->value;
        dest->empty = false;
        map->count++;
    }

    tea_mem_freevec(T, MapEntry, map->entries, map->size);
    map->entries = entries;
    map->size = size;
}

bool tea_map_set(tea_State* T, GCmap* map, TValue key, TValue value)
{
    if(map->count + 1 > map->size * MAP_MAX_LOAD)
    {
        int size = TEA_MEM_GROW(map->size);
        map_adjust_size(T, map, size);
    }

    MapEntry* item = map_find_entry(map->entries, map->size, key);
    bool is_new_key = item->empty;

    if(is_new_key && IS_NULL(item->value))
        map->count++;

    item->key = key;
    item->value = value;
    item->empty = false;

    return is_new_key;
}

bool tea_map_delete(tea_State* T, GCmap* map, TValue key)
{
    if(map->count == 0)
        return false;

    /* Find the entry */
    MapEntry* item = map_find_entry(map->entries, map->size, key);
    if(item->empty)
        return false;

    /* Place a tombstone in the entry */
    item->key = NULL_VAL;
    item->value = TRUE_VAL;
    item->empty = true;

    map->count--;

    if(map->count == 0)
    {
        tea_map_clear(T, map);
    }
    else if(map->count - 1 < map->size * MAP_MAX_LOAD)
    {
        uint32_t size = map->size / 2;
        if(size < 16) size = 16;

        map_adjust_size(T, map, size);
    }

    return true;
}

void tea_map_addall(tea_State* T, GCmap* from, GCmap* to)
{
    for(int i = 0; i < from->size; i++)
    {
        MapEntry* item = &from->entries[i];
        if(!item->empty)
        {
            tea_map_set(T, to, item->key, item->value);
        }
    }
}