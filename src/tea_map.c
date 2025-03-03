/*
** tea_map.c
** Map handling
*/

#include <math.h>

#define tea_map_c
#define TEA_CORE

#include "tea_map.h"
#include "tea_gc.h"
#include "tea_err.h"

/* Create a new map */
GCmap* tea_map_new(tea_State* T)
{
    GCmap* map = tea_mem_newobj(T, GCmap, TEA_TMAP);
    map->count = 0;
    map->size = 0;
    map->entries = NULL;
    return map;
}

/* Free a map */
void TEA_FASTCALL tea_map_free(tea_State* T, GCmap* map)
{
    tea_mem_freevec(T, MapEntry, map->entries, map->size);
    tea_mem_freet(T, map);
}

/* Clear a map */
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

/* Hash an object */
static uint32_t map_hash_obj(TValue* tv)
{
    switch(itype(tv))
    {
        case TEA_TNIL:
            return 1;
        case TEA_TBOOL:
            return 2;
        case TEA_TNUM:
            return map_hash(numV(tv));
        case TEA_TPOINTER:
            return map_hash((uint64_t)pointerV(tv));
        case TEA_TSTR:
            return strV(tv)->hash;
        default:
            return map_hash((uint64_t)gcV(tv));
    }
}

/* Find an entry in the map, skipping over tombstones and deleted entries */
static bool map_find_entry(MapEntry* items, uint32_t size, TValue* key, MapEntry** entry)
{
    uint32_t startidx = map_hash_obj(key) & (size - 1);
    uint32_t idx = startidx;
    MapEntry* tombstone = NULL;

    do
    {
        MapEntry* item = &items[idx];
        if(tvisnil(&item->key))
        {
            if(tvisfalse(&item->val))
            {
                /* Empty item */
                *entry = tombstone != NULL ? tombstone : item;
                return false;
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
            *entry = item;
            return true;
        }

        idx = (idx + 1) & (size - 1);
    }
    while(idx != startidx);
    tea_assertX(tombstone != NULL, "Map should have tombstones or empty entries");
    *entry = tombstone;
    return false;
}

/* Insert an entry into the map */
static bool map_insert_entry(tea_State* T, MapEntry* items, uint32_t size, TValue* key, TValue* val)
{
    tea_assertT(items != NULL, "should ensure size before inserting");
    MapEntry* item;
    if(map_find_entry(items, size, key, &item))
    {
        /* Already present, so just replace the value */
        copyTV(T, &item->val, val);
        return false;
    }
    else
    {
        copyTV(T, &item->key, key);
        copyTV(T, &item->val, val);
        return true;
    }
}

/* -- Map getters ------------------------------------------------------ */

/* Get a key in the map */
cTValue* tea_map_get(GCmap* map, TValue* key)
{
    if(map->count == 0)
        return NULL;

    MapEntry* item;
    if(map_find_entry(map->entries, map->size, key, &item))
        return &item->val;

    return NULL;
}

/* Get a string key in the map */
cTValue* tea_map_getstr(tea_State* T, GCmap* map, GCstr* key)
{
    TValue o;
    setstrV(T, &o, key);
    return tea_map_get(map, &o);
}

#define MAP_MAX_LOAD 0.75
#define MAP_MIN_LOAD 0.25

/* Resize a map to fit the new size */
static void map_resize(tea_State* T, GCmap* map, uint32_t size)
{
    MapEntry* entries = tea_mem_newvec(T, MapEntry, size);
    tea_assertT(entries != NULL, "should ensure size before inserting");
    for(int i = 0; i < size; i++)
    {
        setnilV(&entries[i].key);
        setfalseV(&entries[i].val);
    }

    if(map->size > 0)
    {
        for(int i = 0; i < map->size; i++)
        {
            MapEntry* item = &map->entries[i];
            if(!tvisnil(&item->key))
            {
                MapEntry* entry;
                if(map_find_entry(entries, size, &item->key, &entry))
                {
                    /* Already present, so just replace the value */
                    copyTV(T, &entry->val, &item->val);
                }
                else
                {
                    copyTV(T, &entry->key, &item->key);
                    copyTV(T, &entry->val, &item->val);
                }
                map_insert_entry(T, entries, size, &item->key, &item->val);    
            }
        }
    }

    tea_mem_freevec(T, MapEntry, map->entries, map->size);
    map->entries = entries;
    map->size = size;
}

/* -- Map setters ------------------------------------------------------ */

/* Set a key in the map */
TValue* tea_map_set(tea_State* T, GCmap* map, TValue* key)
{
    if(tvisnil(key))
        tea_err_msg(T, TEA_ERR_NILIDX);
    else if(tvisnum(key) && isnan(numV(key)))
        tea_err_msg(T, TEA_ERR_NANIDX);

    if(map->count + 1 > map->size * MAP_MAX_LOAD)
    {
        uint32_t size = TEA_MEM_GROW(map->size);
        map_resize(T, map, size);
    }

    MapEntry* item;
    if(!map_find_entry(map->entries, map->size, key, &item))
    {
        copyTV(T, &item->key, key);
        map->count++;
    }

    return &item->val;
}

/* Set a string key in the map */
TValue* tea_map_setstr(tea_State* T, GCmap* map, GCstr* str)
{
    TValue o;
    setstrV(T, &o, str);
    return tea_map_set(T, map, &o);
}

/* Copy a map */
GCmap* tea_map_copy(tea_State* T, GCmap* map)
{
    GCmap* m = tea_map_new(T);
    for(int i = 0; i < map->size; i++)
    {
        if(!tvisnil(&map->entries[i].key))
        {
            TValue* o = tea_map_set(T, m, &map->entries[i].key);
            copyTV(T, o, &map->entries[i].val);
        }
    }
    return m;
}

/* Delete an entry from the map */
bool tea_map_delete(tea_State* T, GCmap* map, TValue* key)
{
    if(map->count == 0)
        return false;

    /* Find the entry */
    MapEntry* item;
    if(!map_find_entry(map->entries, map->size, key, &item))
        return false;

    /* Place a tombstone in the entry */
    setnilV(&item->key);
    settrueV(&item->val);
    map->count--;

    if(map->count == 0)
    {
        tea_map_clear(T, map);
    }
    else if(map->size > TEA_MIN_VECSIZE &&
        map->count < (map->size >> 1) * MAP_MIN_LOAD)
    {
        uint32_t size = map->size >> 1;
        if(size < TEA_MIN_VECSIZE) size = TEA_MIN_VECSIZE;
        map_resize(T, map, size);
    }

    return true;
}

/* Merge two maps */
void tea_map_merge(tea_State* T, GCmap* from, GCmap* to)
{
    for(int i = 0; i < from->size; i++)
    {
        MapEntry* item = &from->entries[i];
        if(!tvisnil(&item->key))
        {
            TValue* o = tea_map_set(T, to, &item->key);
            copyTV(T, o, &item->val);
        }
    }
}

/* Get the successor traversal index of a key */
uint32_t map_keyindex(GCmap* map, TValue* key)
{
    if(!tvisnil(key))
    {
        MapEntry* entry;
        if(!map_find_entry(map->entries, map->size, key, &entry))
        {
            return (uint32_t)~0u;
        }
        uint32_t idx = (uint32_t)(entry - map->entries);
        return idx + 1;
    }
    return 0;
}

/* Get the next key/value pair of a map traversal */
int tea_map_next(GCmap* map, TValue* key, TValue* o)
{
    uint32_t idx = map_keyindex(map, key);
    for(; idx < map->size; idx++)
    {
        MapEntry* item = &map->entries[idx];
        if(!tvisnil(&item->key))
        {
            o[0] = item->key;
            o[1] = item->val;
            return 1;
        }
    }
    return (int32_t)idx < 0 ? -1 : 0;
}