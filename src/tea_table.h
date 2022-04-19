#ifndef TEA_TABLE_H
#define TEA_TABLE_H

#include "tea_common.h"
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

void tea_init_table(TeaTable* table);
void tea_free_table(TeaState* state, TeaTable* table);
bool tea_table_get(TeaTable* table, TeaObjectString* key, TeaValue* value);
bool tea_table_set(TeaState* state, TeaTable* table, TeaObjectString* key, TeaValue value);
bool tea_table_delete(TeaTable* table, TeaObjectString* key);
void tea_table_add_all(TeaState* state, TeaTable* from, TeaTable* to);
TeaObjectString* tea_table_find_string(TeaTable* table, const char* chars, int length, uint32_t hash);

void tea_table_remove_white(TeaTable* table);
void tea_mark_table(TeaVM* vm, TeaTable* table);

#endif