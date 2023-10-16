/*
** tea_map.c
** Teascript map implementation
*/

#define tea_map_c
#define TEA_CORE

#include "tea_map.h"

TeaOMap* tea_map_new(TeaState* T)
{
    TeaOMap* map = ALLOCATE_OBJECT(T, TeaOMap, OBJ_MAP);
    map->count = 0;
    map->capacity = 0;
    map->items = NULL;

    return map;
}

void tea_map_clear(TeaState* T, TeaOMap* map)
{
    TEA_FREE_ARRAY(T, TeaMapItem, map->items, map->capacity);
    map->items = NULL;
    map->capacity = 0;
    map->count = 0;
}

static inline uint32_t hash_bits(uint64_t hash)
{
    /* From v8's ComputeLongHash() which in turn cites:
    ** Thomas Wang, Integer Hash Functions.
    ** http://www.concentric.net/~Ttwang/tech/inthash.htm
    */
    hash = ~hash + (hash << 18);  /* hash = (hash << 18) - hash - 1; */
    hash = hash ^ (hash >> 31);
    hash = hash * 21;  /* hash = (hash + (hash << 2)) + (hash << 4); */
    hash = hash ^ (hash >> 11);
    hash = hash + (hash << 6);
    hash = hash ^ (hash >> 22);
    return (uint32_t)(hash & 0x3fffffff);
}

static inline uint32_t hash_number(double number)
{
#ifdef TEA_NAN_TAGGING
    return hash_bits(num_to_value(number));
#else
    return hash_bits(number);
#endif
}

static uint32_t hash_object(TeaObject* object)
{
    switch(object->type)
    {
        case OBJ_STRING:
            return ((TeaOString*)object)->hash;

        default: return 0;
    }
}

static uint32_t hash_value(TeaValue value)
{
#ifdef TEA_NAN_TAGGING
    if(IS_OBJECT(value)) return hash_object(AS_OBJECT(value));

    /* Hash the raw bits of the unboxed value */
    return hash_bits(value);
#else
    switch(value.type)
    {
        case VAL_NULL:  return 1;
        case VAL_NUMBER:   return hash_number(AS_NUMBER(value));
        case VAL_BOOL:  return 2;
        case VAL_OBJECT:   return hash_object(AS_OBJECT(value));
        default:;
    }
    return 0;
#endif
}

static TeaMapItem* map_find_entry(TeaMapItem* items, int capacity, TeaValue key)
{
    uint32_t hash = hash_value(key);
    uint32_t index = hash & (capacity - 1);
    TeaMapItem* tombstone = NULL;

    while(true)
    {
        TeaMapItem* item = &items[index];
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
#ifdef TEA_NAN_TAGGING
        else if(item->key == key)
#else
        else if(tea_val_rawequal(item->key, key))
#endif
        {
            /* We found the key */
            return item;
        }

        index = (index + 1) & (capacity - 1);
    }
}

bool tea_map_get(TeaOMap* map, TeaValue key, TeaValue* value)
{
    if(map->count == 0)
        return false;

    TeaMapItem* item = map_find_entry(map->items, map->capacity, key);
    if(item->empty)
        return false;

    *value = item->value;

    return true;
}

#define MAP_MAX_LOAD 0.75

static void map_adjust_size(TeaState* T, TeaOMap* map, int capacity)
{
    TeaMapItem* items = TEA_ALLOCATE(T, TeaMapItem, capacity);
    for(int i = 0; i < capacity; i++)
    {
        items[i].key = NULL_VAL;
        items[i].value = NULL_VAL;
        items[i].empty = true;
    }

    map->count = 0;
    for(int i = 0; i < map->capacity; i++)
    {
        TeaMapItem* item = &map->items[i];
        if(item->empty)
            continue;

        TeaMapItem* dest = map_find_entry(items, capacity, item->key);
        dest->key = item->key;
        dest->value = item->value;
        dest->empty = false;
        map->count++;
    }

    TEA_FREE_ARRAY(T, TeaMapItem, map->items, map->capacity);
    map->items = items;
    map->capacity = capacity;
}

bool tea_map_set(TeaState* T, TeaOMap* map, TeaValue key, TeaValue value)
{
    if(map->count + 1 > map->capacity * MAP_MAX_LOAD)
    {
        int capacity = TEA_GROW_CAPACITY(map->capacity);
        map_adjust_size(T, map, capacity);
    }

    TeaMapItem* item = map_find_entry(map->items, map->capacity, key);
    bool is_new_key = item->empty;

    if(is_new_key && IS_NULL(item->value))
        map->count++;

    item->key = key;
    item->value = value;
    item->empty = false;

    return is_new_key;
}

bool tea_map_delete(TeaState* T, TeaOMap* map, TeaValue key)
{
    if(map->count == 0)
        return false;

    /* Find the entry */
    TeaMapItem* item = map_find_entry(map->items, map->capacity, key);
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
    else if(map->count - 1 < map->capacity * MAP_MAX_LOAD)
    {
        uint32_t capacity = map->capacity / 2;
        if(capacity < 16) capacity = 16;

        map_adjust_size(T, map, capacity);
    }

    return true;
}

void tea_map_add_all(TeaState* T, TeaOMap* from, TeaOMap* to)
{
    for(int i = 0; i < from->capacity; i++)
    {
        TeaMapItem* item = &from->items[i];
        if(!item->empty)
        {
            tea_map_set(T, to, item->key, item->value);
        }
    }
}