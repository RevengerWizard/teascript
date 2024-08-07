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
    GCmap* map = tea_mem_newobj(T, GCmap, TEA_TMAP);
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

/* -- Object hashing ------------------------------------------------------ */

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

static uint32_t map_hash_obj(TValue* value)
{
    switch(itype(value))
    {
        case TEA_TNIL:
            return 1;
        case TEA_TBOOL:
            return 2;
        case TEA_TNUM:
            return map_hash(numV(value));
        case TEA_TPOINTER:
            return map_hash((uint64_t)pointerV(value));
        case TEA_TSTR:
            return strV(value)->hash;
        default:
            return map_hash((uint64_t)gcV(value));
    }
}

static MapEntry* map_find_entry(MapEntry* items, int size, TValue* key)
{
    uint32_t hash = map_hash_obj(key);
    uint32_t idx = hash & (size - 1);
    MapEntry* tombstone = NULL;

    while(true)
    {
        MapEntry* item = &items[idx];
        if(item->empty)
        {
            if(tvisnil(&item->val))
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
        else if(tea_obj_rawequal(&item->key, key))
        {
            /* We found the key */
            return item;
        }

        idx = (idx + 1) & (size - 1);
    }
}

/* -- Map getters ------------------------------------------------------ */

cTValue* tea_map_get(GCmap* map, TValue* key)
{
    if(map->count == 0)
        return NULL;

    MapEntry* item = map_find_entry(map->entries, map->size, key);
    if(item->empty)
        return NULL;

    return &item->val;
}

cTValue* tea_map_getstr(tea_State* T, GCmap* map, GCstr* key)
{
    TValue o;
    setstrV(T, &o, key);
    return tea_map_get(map, &o);
}

#define MAP_MAX_LOAD 0.75

/* Resize a map to fit the new size */
static void map_resize(tea_State* T, GCmap* map, int size)
{
    MapEntry* entries = tea_mem_newvec(T, MapEntry, size);
    for(int i = 0; i < size; i++)
    {
        setnilV(&entries[i].key);
        setnilV(&entries[i].val);
        entries[i].empty = true;
    }

    map->count = 0;
    for(int i = 0; i < map->size; i++)
    {
        MapEntry* item = &map->entries[i];
        if(item->empty)
            continue;

        MapEntry* dest = map_find_entry(entries, size, &item->key);
        dest->key = item->key;
        dest->val = item->val;
        dest->empty = false;
        map->count++;
    }

    tea_mem_freevec(T, MapEntry, map->entries, map->size);
    map->entries = entries;
    map->size = size;
}

/* -- Map setters ------------------------------------------------------ */

TValue* tea_map_set(tea_State* T, GCmap* map, TValue* key)
{
    if(map->count + 1 > map->size * MAP_MAX_LOAD)
    {
        int size = TEA_MEM_GROW(map->size);
        map_resize(T, map, size);
    }

    MapEntry* item = map_find_entry(map->entries, map->size, key);
    bool is_new_key = item->empty;

    if(is_new_key && tvisnil(&item->val))
        map->count++;

    copyTV(T, &item->key, key);
    item->empty = false;

    return &item->val;
}

TValue* tea_map_setstr(tea_State* T, GCmap* map, GCstr* str)
{
    TValue o;
    setstrV(T, &o, str);
    return tea_map_set(T, map, &o);
}

GCmap* tea_map_copy(tea_State* T, GCmap* map)
{
    GCmap* m = tea_map_new(T);
    for(int i = 0; i < map->size; i++)
    {
        if(map->entries[i].empty) continue;
        TValue* o = tea_map_set(T, m, &map->entries[i].key);
        copyTV(T, o, &map->entries[i].val);
    }
    return m;
}

bool tea_map_delete(tea_State* T, GCmap* map, TValue* key)
{
    if(map->count == 0)
        return false;

    /* Find the entry */
    MapEntry* item = map_find_entry(map->entries, map->size, key);
    if(item->empty)
        return false;

    /* Place a tombstone in the entry */
    setnilV(&item->key);
    setnilV(&item->val);
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
        map_resize(T, map, size);
    }

    return true;
}

void tea_map_merge(tea_State* T, GCmap* from, GCmap* to)
{
    for(int i = 0; i < from->size; i++)
    {
        MapEntry* item = &from->entries[i];
        if(!item->empty)
        {
            TValue* o = tea_map_set(T, to, &item->key);
            copyTV(T, o, &item->val);
        }
    }
}