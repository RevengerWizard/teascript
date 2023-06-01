/*
** tea_table.h
** Teascript hash table
*/

#ifndef TEA_TABLE_H
#define TEA_TABLE_H

#include "tea_def.h"
#include "tea_value.h"

#define TABLE_MAX_LOAD 0.75

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

void teaT_init(TeaTable* table);
void teaT_free(TeaState* T, TeaTable* table);
bool teaT_get(TeaTable* table, TeaObjectString* key, TeaValue* value);
bool teaT_set(TeaState* T, TeaTable* table, TeaObjectString* key, TeaValue value);
bool teaT_delete(TeaTable* table, TeaObjectString* key);
void teaT_add_all(TeaState* T, TeaTable* from, TeaTable* to);
TeaObjectString* teaT_find_string(TeaTable* table, const char* chars, int length, uint32_t hash);

void teaT_remove_white(TeaTable* table);
void teaT_mark(TeaState* T, TeaTable* table);

#endif