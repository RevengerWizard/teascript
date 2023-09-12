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
    TeaObjectString* key;
    TeaValue value;
} TeaEntry;

typedef struct
{
    int count;
    int capacity;
    TeaEntry* entries;
} TeaTable;

TEA_FUNC void tea_table_init(TeaTable* table);
TEA_FUNC void tea_table_free(TeaState* T, TeaTable* table);
TEA_FUNC bool tea_table_get(TeaTable* table, TeaObjectString* key, TeaValue* value);
TEA_FUNC bool tea_table_set(TeaState* T, TeaTable* table, TeaObjectString* key, TeaValue value);
TEA_FUNC bool tea_table_delete(TeaTable* table, TeaObjectString* key);
TEA_FUNC void tea_table_add_all(TeaState* T, TeaTable* from, TeaTable* to);
TEA_FUNC TeaObjectString* tea_table_find_string(TeaTable* table, const char* chars, int length, uint32_t hash);

TEA_FUNC void tea_table_remove_white(TeaTable* table);
TEA_FUNC void tea_table_mark(TeaState* T, TeaTable* table);

#endif