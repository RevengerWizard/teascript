/*
** tea_map.h
** Map handling
*/

#ifndef _TEA_MAP_H
#define _TEA_MAP_H

#include "tea_obj.h"

TEA_FUNC GCmap* tea_map_new(tea_State* T);
TEA_FUNC void TEA_FASTCALL tea_map_free(tea_State* T, GCmap* map);
TEA_FUNC void tea_map_clear(tea_State* T, GCmap* map);
TEA_FUNC TValue* tea_map_set(tea_State* T, GCmap* map, TValue* key);
TEA_FUNC TValue* tea_map_setstr(tea_State* T, GCmap* map, GCstr* str);
TEA_FUNC cTValue* tea_map_get(GCmap* map, TValue* key);
TEA_FUNC cTValue* tea_map_getstr(tea_State* T, GCmap* map, GCstr* key);
TEA_FUNC GCmap* tea_map_copy(tea_State* T, GCmap* map);
TEA_FUNC bool tea_map_delete(tea_State* T, GCmap* map, TValue* key);
TEA_FUNC void tea_map_merge(tea_State* T, GCmap* from, GCmap* to);
TEA_FUNC int tea_map_next(GCmap* map, TValue* key, TValue* o);

#endif