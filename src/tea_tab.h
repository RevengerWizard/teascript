/*
** tea_tab.h
** Hash table handling
*/

#ifndef _TEA_TAB_H
#define _TEA_TAB_H

#include "tea_def.h"
#include "tea_obj.h"

TEA_FUNC void tea_tab_init(Tab* tab);
TEA_FUNC void tea_tab_free(tea_State* T, Tab* tab);
TEA_FUNC TValue* tea_tab_get(Tab* tab, GCstr* key);
TEA_FUNC TValue* tea_tab_set(tea_State* T, Tab* tab, GCstr* key);
TEA_FUNC bool tea_tab_delete(Tab* tab, GCstr* key);
TEA_FUNC void tea_tab_merge(tea_State* T, Tab* from, Tab* to);

#endif