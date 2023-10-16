/*
** tea_table.c
** Teascript hash table
*/

#include <stdlib.h>
#include <string.h>

#define tea_table_c
#define TEA_CORE

#include "tea_gc.h"
#include "tea_memory.h"
#include "tea_object.h"
#include "tea_table.h"
#include "tea_value.h"

void tea_tab_init(TeaTable* table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void tea_tab_free(TeaState* T, TeaTable* table)
{
    TEA_FREE_ARRAY(T, TeaEntry, table->entries, table->capacity);
    tea_tab_init(table);
}

static TeaEntry* find_entry(TeaEntry* entries, int capacity, TeaOString* key)
{
    uint32_t index = key->hash & (capacity - 1);
    TeaEntry* tombstone = NULL;

    while(true)
    {
        TeaEntry* entry = &entries[index];
        if(entry->key == NULL)
        {
            if(IS_NULL(entry->value))
            {
                /* Empty entry */
                return tombstone != NULL ? tombstone : entry;
            }
            else
            {
                /* We found a tombstone */
                if(tombstone == NULL)
                    tombstone = entry;
            }
        }
        else if(entry->key == key)
        {
            /* We found the key */
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

bool tea_tab_get(TeaTable* table, TeaOString* key, TeaValue* value)
{
    if(table->count == 0)
        return false;

    TeaEntry* entry = find_entry(table->entries, table->capacity, key);
    if(entry->key == NULL)
        return false;

    *value = entry->value;

    return true;
}

static void adjust_capacity(TeaState* T, TeaTable* table, int capacity)
{
    TeaEntry* entries = TEA_ALLOCATE(T, TeaEntry, capacity);
    for(int i = 0; i < capacity; i++)
    {
        entries[i].key = NULL;
        entries[i].value = NULL_VAL;
    }

    table->count = 0;
    for(int i = 0; i < table->capacity; i++)
    {
        TeaEntry *entry = &table->entries[i];
        if(entry->key == NULL)
            continue;

        TeaEntry* dest = find_entry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    TEA_FREE_ARRAY(T, TeaEntry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

#define TABLE_MAX_LOAD 0.75

bool tea_tab_set(TeaState* T, TeaTable* table, TeaOString* key, TeaValue value)
{
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD)
    {
        int capacity = TEA_GROW_CAPACITY(table->capacity);
        adjust_capacity(T, table, capacity);
    }

    TeaEntry* entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = entry->key == NULL;

    if(is_new_key && IS_NULL(entry->value))
        table->count++;

    entry->key = key;
    entry->value = value;

    return is_new_key;
}

bool tea_tab_delete(TeaTable* table, TeaOString* key)
{
    if(table->count == 0)
        return false;

    /* Find the entry */
    TeaEntry* entry = find_entry(table->entries, table->capacity, key);
    if(entry->key == NULL)
        return false;

    /* Place a tombstone in the entry */
    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

void tea_tab_addall(TeaState* T, TeaTable* from, TeaTable* to)
{
    for(int i = 0; i < from->capacity; i++)
    {
        TeaEntry* entry = &from->entries[i];
        if(entry->key != NULL)
        {
            tea_tab_set(T, to, entry->key, entry->value);
        }
    }
}

TeaOString* tea_tab_findstr(TeaTable* table, const char* chars, int length, uint32_t hash)
{
    if(table->count == 0)
        return NULL;

    uint32_t index = hash & (table->capacity - 1);
    while(true)
    {
        TeaEntry* entry = &table->entries[index];
        if(entry->key == NULL)
        {
            /* Stop if we find an empty non-tombstone entry */
            if (IS_NULL(entry->value))
                return NULL;
        }
        else if(entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0)
        {
            /* We found it */
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void tea_tab_remove_white(TeaTable* table)
{
    for(int i = 0; i < table->capacity; i++)
    {
        TeaEntry* entry = &table->entries[i];
        if(entry->key != NULL && !entry->key->obj.is_marked)
        {
            tea_tab_delete(table, entry->key);
        }
    }
}

void tea_tab_mark(TeaState* T, TeaTable* table)
{
    for(int i = 0; i < table->capacity; i++)
    {
        TeaEntry* entry = &table->entries[i];
        tea_gc_markobj(T, (TeaObject*)entry->key);
        tea_gc_markval(T, entry->value);
    }
}