#include <stdlib.h>
#include <string.h>

#include "memory/tea_memory.h"
#include "vm/tea_object.h"
#include "util/tea_table.h"
#include "vm/tea_value.h"

#define TABLE_MAX_LOAD 0.75

void tea_init_table(TeaTable* table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void tea_free_table(TeaTable* table)
{
    FREE_ARRAY(TeaEntry, table->entries, table->capacity);
    tea_init_table(table);
}

static TeaEntry* find_entry(TeaEntry* entries, int capacity, TeaObjectString* key)
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
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            }
            else
            {
                // We found a tombstone.
                if(tombstone == NULL)
                    tombstone = entry;
            }
        }
        else if(entry->key == key)
        {
            // We found the key.
            return entry;
        }

        index = (index + 1) & (capacity - 1);
    }
}

bool tea_table_get(TeaTable* table, TeaObjectString* key, TeaValue* value)
{
    if(table->count == 0)
        return false;

    TeaEntry* entry = find_entry(table->entries, table->capacity, key);
    if(entry->key == NULL)
        return false;

    *value = entry->value;

    return true;
}

static void adjust_capacity(TeaTable* table, int capacity)
{
    TeaEntry* entries = ALLOCATE(TeaEntry, capacity);
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

    FREE_ARRAY(TeaEntry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tea_table_set(TeaTable* table, TeaObjectString* key, TeaValue value)
{
    if(table->count + 1 > table->capacity * TABLE_MAX_LOAD)
    {
        int capacity = GROW_CAPACITY(table->capacity);
        adjust_capacity(table, capacity);
    }

    TeaEntry* entry = find_entry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;

    if(isNewKey && IS_NULL(entry->value))
        table->count++;

    entry->key = key;
    entry->value = value;
    
    return isNewKey;
}

bool tea_table_delete(TeaTable* table, TeaObjectString* key)
{
    if(table->count == 0)
        return false;

    // Find the entry.
    TeaEntry* entry = find_entry(table->entries, table->capacity, key);
    if(entry->key == NULL)
        return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = BOOL_VAL(true);

    return true;
}

void tea_table_add_all(TeaTable* from, TeaTable* to)
{
    for(int i = 0; i < from->capacity; i++)
    {
        TeaEntry* entry = &from->entries[i];
        if(entry->key != NULL)
        {
            tea_table_set(to, entry->key, entry->value);
        }
    }
}

TeaObjectString* tea_table_find_string(TeaTable* table, const char* chars, int length, uint32_t hash)
{
    if(table->count == 0)
        return NULL;

    uint32_t index = hash & (table->capacity - 1);
    while(true)
    {
        TeaEntry* entry = &table->entries[index];
        if(entry->key == NULL)
        {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NULL(entry->value))
                return NULL;
        }
        else if(entry->key->length == length && entry->key->hash == hash && memcmp(entry->key->chars, chars, length) == 0)
        {
            // We found it.
            return entry->key;
        }

        index = (index + 1) & (table->capacity - 1);
    }
}

void tea_table_remove_white(TeaTable* table)
{
    for(int i = 0; i < table->capacity; i++)
    {
        TeaEntry* entry = &table->entries[i];
        if(entry->key != NULL && !entry->key->obj.is_marked)
        {
            tea_table_delete(table, entry->key);
        }
    }
}

void tea_mark_table(TeaTable* table)
{
    for(int i = 0; i < table->capacity; i++)
    {
        TeaEntry* entry = &table->entries[i];
        tea_mark_object((TeaObject*)entry->key);
        tea_mark_value(entry->value);
    }
}