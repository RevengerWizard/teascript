/*
** tea_tab.h
** Hash table handling
*/

#ifndef _TEA_TAB_H
#define _TEA_TAB_H

#include "tea_def.h"
#include "tea_obj.h"

TEA_FUNC void tea_tab_init(Table* table);
TEA_FUNC void tea_tab_free(tea_State* T, Table* table);
TEA_FUNC bool tea_tab_get(Table* table, GCstr* key, Value* value);
TEA_FUNC bool tea_tab_set(tea_State* T, Table* table, GCstr* key, Value value);
TEA_FUNC bool tea_tab_delete(Table* table, GCstr* key);
TEA_FUNC void tea_tab_addall(tea_State* T, Table* from, Table* to);
TEA_FUNC GCstr* tea_tab_findstr(Table* table, const char* chars, int len, uint32_t hash);

TEA_FUNC void tea_tab_white(Table* table);
TEA_FUNC void tea_tab_mark(tea_State* T, Table* table);

#endif