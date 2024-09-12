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

void tea_tab_init(Tab* tab)
{
    tab->count = 0;
    tab->size = 0;
    tab->entries = NULL;
}

void tea_tab_free(tea_State* T, Tab* tab)
{
    tea_mem_freevec(T, TabEntry, tab->entries, tab->size);
    tea_tab_init(tab);
}

static TabEntry* tab_find_entry(TabEntry* entries, int size, GCstr* key)
{
    uint32_t idx = key->hash & (size - 1);
    TabEntry* tombstone = NULL;

    while(true)
    {
        TabEntry* entry = &entries[idx];
        if(entry->key == NULL)
        {
            if(tvisnil(&entry->val))
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

        idx = (idx + 1) & (size - 1);
    }
}

TValue* tea_tab_get(Tab* tab, GCstr* key)
{
    if(tab->count == 0)
        return NULL;

    TabEntry* entry = tab_find_entry(tab->entries, tab->size, key);
    if(entry->key == NULL)
        return NULL;

    return &entry->val;
}

static void tab_resize(tea_State* T, Tab* tab, int size)
{
    TabEntry* entries = tea_mem_newvec(T, TabEntry, size);
    for(int i = 0; i < size; i++)
    {
        entries[i].key = NULL;
        setnilV(&entries[i].val);
    }

    tab->count = 0;
    for(int i = 0; i < tab->size; i++)
    {
        TabEntry *entry = &tab->entries[i];
        if(entry->key == NULL)
            continue;

        TabEntry* dest = tab_find_entry(entries, size, entry->key);
        dest->key = entry->key;
        dest->val = entry->val;
        tab->count++;
    }

    tea_mem_freevec(T, TabEntry, tab->entries, tab->size);
    tab->entries = entries;
    tab->size = size;
}

#define TABLE_MAX_LOAD 0.75

TValue* tea_tab_set(tea_State* T, Tab* tab, GCstr* key, bool* b)
{
    if(tab->count + 1 > tab->size * TABLE_MAX_LOAD)
    {
        int size = TEA_MEM_GROW(tab->size);
        tab_resize(T, tab, size);
    }

    TabEntry* entry = tab_find_entry(tab->entries, tab->size, key);
    bool is_new_key = entry->key == NULL;

    if(is_new_key && tvisnil(&entry->val))
        tab->count++;

    entry->key = key;

    if(b) *b = is_new_key;

    return &entry->val;
}

bool tea_tab_delete(Tab* tab, GCstr* key)
{
    if(tab->count == 0)
        return false;

    /* Find the entry */
    TabEntry* entry = tab_find_entry(tab->entries, tab->size, key);
    if(entry->key == NULL)
        return false;

    /* Place a tombstone in the entry */
    entry->key = NULL;
    settrueV(&entry->val);

    return true;
}

void tea_tab_merge(tea_State* T, Tab* from, Tab* to)
{
    for(int i = 0; i < from->size; i++)
    {
        TabEntry* entry = &from->entries[i];
        if(entry->key != NULL)
        {
            TValue* o = tea_tab_set(T, to, entry->key, NULL);
            copyTV(T, o, &entry->val);
        }
    }
}

GCstr* tea_tab_findstr(Tab* tab, const char* chars, int len, StrHash hash)
{
    if(tab->count == 0)
        return NULL;

    uint32_t idx = hash & (tab->size - 1);
    while(true)
    {
        TabEntry* entry = &tab->entries[idx];
        if(entry->key == NULL)
        {
            /* Stop if we find an empty non-tombstone entry */
            if(tvisnil(&entry->val))
                return NULL;
        }
        else if(entry->key->len == len && entry->key->hash == hash && memcmp(str_data(entry->key), chars, len) == 0)
        {
            /* We found it */
            return entry->key;
        }

        idx = (idx + 1) & (tab->size - 1);
    }
}

void tea_tab_white(Tab* tab)
{
    for(int i = 0; i < tab->size; i++)
    {
        TabEntry* entry = &tab->entries[i];
        if(entry->key != NULL && !entry->key->obj.marked)
        {
            tea_tab_delete(tab, entry->key);
        }
    }
}

void tea_tab_mark(tea_State* T, Tab* tab)
{
    for(int i = 0; i < tab->size; i++)
    {
        TabEntry* entry = &tab->entries[i];
        tea_gc_markobj(T, (GCobj*)entry->key);
        tea_gc_markval(T, &entry->val);
    }
}