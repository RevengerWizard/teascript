/*
** tea_table.h
** Teascript hash table
*/

#ifndef TEA_TABLE_H
#define TEA_TABLE_H

#include "tea_def.h"
#include "tea_value.h"

typedef struct
{
    TeaOString* key;
    TeaValue value;
} TeaEntry;

typedef struct
{
    int count;
    int capacity;
    TeaEntry* entries;
} TeaTable;

TEA_FUNC void tea_tab_init(TeaTable* table);
TEA_FUNC void tea_tab_free(TeaState* T, TeaTable* table);
TEA_FUNC bool tea_tab_get(TeaTable* table, TeaOString* key, TeaValue* value);
TEA_FUNC bool tea_tab_set(TeaState* T, TeaTable* table, TeaOString* key, TeaValue value);
TEA_FUNC bool tea_tab_delete(TeaTable* table, TeaOString* key);
TEA_FUNC void tea_tab_addall(TeaState* T, TeaTable* from, TeaTable* to);
TEA_FUNC TeaOString* tea_tab_findstr(TeaTable* table, const char* chars, int length, uint32_t hash);

TEA_FUNC void tea_tab_remove_white(TeaTable* table);
TEA_FUNC void tea_tab_mark(TeaState* T, TeaTable* table);

#endif