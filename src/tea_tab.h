/*
** tea_tab.h
** Hash table handling
*/

#ifndef _TEA_TAB_H
#define _TEA_TAB_H

#include "tea_def.h"
#include "tea_obj.h"

TEA_FUNC void tea_tab_init(Table* tab);
TEA_FUNC void tea_tab_free(tea_State* T, Table* tab);
TEA_FUNC TValue* tea_tab_get(Table* tab, GCstr* key);
TEA_FUNC TValue* tea_tab_set(tea_State* T, Table* tab, GCstr* key, bool* b);
TEA_FUNC bool tea_tab_delete(Table* tab, GCstr* key);
TEA_FUNC void tea_tab_merge(tea_State* T, Table* from, Table* to);
TEA_FUNC GCstr* tea_tab_findstr(Table* tab, const char* chars, int len, StrHash hash);

TEA_FUNC void tea_tab_white(Table* tab);
TEA_FUNC void tea_tab_mark(tea_State* T, Table* tab);

#endif