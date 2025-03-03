/*
** tea_tab.c
** Hash table handling
*/

#define tea_tab_c
#define TEA_CORE

#include "tea_gc.h"
#include "tea_tab.h"

/* Initialize a table */
void tea_tab_init(Tab* tab)
{
    tab->count = 0;
    tab->size = 0;
    tab->entries = NULL;
}

/* Free a table */
void tea_tab_free(tea_State* T, Tab* tab)
{
    tea_mem_freevec(T, TabEntry, tab->entries, tab->size);
    tab->count = 0;
    tab->size = 0;
    tab->entries = NULL;
}

/* Find an entry in the table */
static TabEntry* tab_findkey(TabEntry* entries, uint32_t size, GCstr* key)
{
    uint32_t idx = key->hash & (size - 1);
    TabEntry* tombstone = NULL;

    while(true)
    {
        TabEntry* entry = &entries[idx];
        if(entry->key == NULL)
        {
            if(tvisnil(&entry->u.val))
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

/* Get a key in the table */
TValue* tea_tab_get(Tab* tab, GCstr* key)
{
    if(tab->count == 0)
        return NULL;

    TabEntry* entry = tab_findkey(tab->entries, tab->size, key);
    if(entry->key == NULL)
        return NULL;

    return &entry->u.val;
}

/* Resize a table to fit the new size */
static void tab_resize(tea_State* T, Tab* tab, uint32_t size)
{
    TabEntry* entries = tea_mem_newvec(T, TabEntry, size);
    for(uint32_t i = 0; i < size; i++)
    {
        entries[i].key = NULL;
        entries[i].flags = 0;
        setnilV(&entries[i].u.val);
    }

    tab->count = 0;
    for(uint32_t i = 0; i < tab->size; i++)
    {
        TabEntry* entry = &tab->entries[i];
        if(entry->key != NULL)
        {
            TabEntry* dest = tab_findkey(entries, size, entry->key);
            dest->key = entry->key;
            dest->flags = entry->flags;
            if(entry->flags & (ACC_GET | ACC_SET))
            {
                dest->u.acc = entry->u.acc;
            }
            else
            {
                copyTV(T, &dest->u.val, &entry->u.val);
            }
            tab->count++;
        }
    }

    tea_mem_freevec(T, TabEntry, tab->entries, tab->size);
    tab->entries = entries;
    tab->size = size;
}

#define TABLE_MAX_LOAD 0.75

/* Set a key in the table */
TValue* tea_tab_set(tea_State* T, Tab* tab, GCstr* key)
{
    if(tab->count + 1 > tab->size * TABLE_MAX_LOAD)
    {
        uint32_t size = TEA_MEM_GROW(tab->size);
        tab_resize(T, tab, size);
    }

    TabEntry* entry = tab_findkey(tab->entries, tab->size, key);
    if((entry->key == NULL) && tvisnil(&entry->u.val))
        tab->count++;

    entry->key = key;
    return &entry->u.val;
}

/* Get a key in the table with accessor flags */
TValue* tea_tab_getx(Tab* tab, GCstr* key, uint8_t* flags)
{
    if(tab->count == 0)
        return NULL;
    
    TabEntry* entry = tab_findkey(tab->entries, tab->size, key);
    if(entry->key == NULL)
        return NULL;

    uint8_t old = *flags;
    *flags = entry->flags;
    if(entry->flags & (ACC_GET | ACC_SET))
    {
        if((old & ACC_SET) && (entry->flags & ACC_SET))
        {
            return &entry->u.acc.set;
        }
        return (entry->flags & ACC_GET) ? &entry->u.acc.get : &entry->u.acc.set;
    }
    return &entry->u.val;
}

/* Set a key in the table with accessor flags */
TValue* tea_tab_setx(tea_State* T, Tab* tab, GCstr* key, uint8_t flags)
{
    if(tab->count + 1 > tab->size * TABLE_MAX_LOAD)
    {
        uint32_t size = TEA_MEM_GROW(tab->size);
        tab_resize(T, tab, size);
    }

    TabEntry* entry = tab_findkey(tab->entries, tab->size, key);
    bool isnew = (entry->key == NULL) && tvisnil(&entry->u.val);
    if(isnew)
        tab->count++;

    entry->key = key;
    if(flags & (ACC_GET | ACC_SET))
    {
        if(!isnew)
            entry->flags |= flags;
        else
            entry->flags = flags;
            
        if(flags & ACC_GET)
            return &entry->u.acc.get;
        else
            return &entry->u.acc.set;
    }
    else
    {
        entry->flags = flags;
        return &entry->u.val;
    }
}

/* Delete an entry from the table */
bool tea_tab_delete(Tab* tab, GCstr* key)
{
    if(tab->count == 0)
        return false;

    /* Find the entry */
    TabEntry* entry = tab_findkey(tab->entries, tab->size, key);
    if(entry->key == NULL)
        return false;

    /* Place a tombstone in the entry */
    entry->key = NULL;
    entry->flags = 0;
    settrueV(&entry->u.val);
    return true;
}

/* Merge a table into another */
void tea_tab_merge(tea_State* T, Tab* from, Tab* to)
{
    for(uint32_t i = 0; i < from->size; i++)
    {
        TabEntry* entry = &from->entries[i];
        if(entry->key != NULL)
        {
            TValue* o = tea_tab_set(T, to, entry->key);
            copyTV(T, o, &entry->u.val);
        }
    }
}