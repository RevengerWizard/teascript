/*
** tea_tab.c
** Hash table handling
*/

#include <stdlib.h>
#include <string.h>

#define tea_tab_c
#define TEA_CORE

#include "tea_gc.h"
#include "tea_tab.h"

void tea_tab_init(Table* table)
{
    table->count = 0;
    table->size = 0;
    table->entries = NULL;
}

void tea_tab_free(tea_State* T, Table* table)
{
    tea_mem_freevec(T, TableEntry, table->entries, table->size);
    tea_tab_init(table);
}

static TableEntry* tab_find_entry(TableEntry* entries, int size, GCstr* key)
{
    uint32_t index = key->hash & (size - 1);
    TableEntry* tombstone = NULL;

    while(true)
    {
        TableEntry* entry = &entries[index];
        if(entry->key == NULL)
        {
            if(tvisnil(&entry->value))
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

        index = (index + 1) & (size - 1);
    }
}

TValue* tea_tab_get(Table* table, GCstr* key)
{
    if(table->count == 0)
        return NULL;

    TableEntry* entry = tab_find_entry(table->entries, table->size, key);
    if(entry->key == NULL)
        return NULL;

    return &entry->value;
}

static void tab_resize(tea_State* T, Table* table, int size)
{
    TableEntry* entries = tea_mem_newvec(T, TableEntry, size);
    for(int i = 0; i < size; i++)
    {
        entries[i].key = NULL;
        setnilV(&entries[i].value);
    }

    table->count = 0;
    for(int i = 0; i < table->size; i++)
    {
        TableEntry *entry = &table->entries[i];
        if(entry->key == NULL)
            continue;

        TableEntry* dest = tab_find_entry(entries, size, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    tea_mem_freevec(T, TableEntry, table->entries, table->size);
    table->entries = entries;
    table->size = size;
}

#define TABLE_MAX_LOAD 0.75

TValue* tea_tab_set(tea_State* T, Table* table, GCstr* key, bool* b)
{
    if(table->count + 1 > table->size * TABLE_MAX_LOAD)
    {
        int size = TEA_MEM_GROW(table->size);
        tab_resize(T, table, size);
    }

    TableEntry* entry = tab_find_entry(table->entries, table->size, key);
    bool is_new_key = entry->key == NULL;

    if(is_new_key && tvisnil(&entry->value))
        table->count++;

    entry->key = key;

    if(b) *b = is_new_key;

    return &entry->value;
}

bool tea_tab_delete(Table* table, GCstr* key)
{
    if(table->count == 0)
        return false;

    /* Find the entry */
    TableEntry* entry = tab_find_entry(table->entries, table->size, key);
    if(entry->key == NULL)
        return false;

    /* Place a tombstone in the entry */
    entry->key = NULL;
    settrueV(&entry->value);

    return true;
}

void tea_tab_merge(tea_State* T, Table* from, Table* to)
{
    for(int i = 0; i < from->size; i++)
    {
        TableEntry* entry = &from->entries[i];
        if(entry->key != NULL)
        {
            TValue* o = tea_tab_set(T, to, entry->key, NULL);
            copyTV(T, o, &entry->value);
        }
    }
}

GCstr* tea_tab_findstr(Table* table, const char* chars, int len, StrHash hash)
{
    if(table->count == 0)
        return NULL;

    uint32_t index = hash & (table->size - 1);
    while(true)
    {
        TableEntry* entry = &table->entries[index];
        if(entry->key == NULL)
        {
            /* Stop if we find an empty non-tombstone entry */
            if(tvisnil(&entry->value))
                return NULL;
        }
        else if(entry->key->len == len && entry->key->hash == hash && memcmp(str_data(entry->key), chars, len) == 0)
        {
            /* We found it */
            return entry->key;
        }

        index = (index + 1) & (table->size - 1);
    }
}

void tea_tab_white(Table* table)
{
    for(int i = 0; i < table->size; i++)
    {
        TableEntry* entry = &table->entries[i];
        if(entry->key != NULL && !entry->key->obj.marked)
        {
            tea_tab_delete(table, entry->key);
        }
    }
}

void tea_tab_mark(tea_State* T, Table* table)
{
    for(int i = 0; i < table->size; i++)
    {
        TableEntry* entry = &table->entries[i];
        tea_gc_markobj(T, (GCobj*)entry->key);
        tea_gc_markval(T, &entry->value);
    }
}