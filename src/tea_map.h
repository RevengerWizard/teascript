/*
** tea_map.h
** Map handling
*/

#ifndef _TEA_MAP_H
#define _TEA_MAP_H

#include "tea_obj.h"

TEA_FUNC GCmap* tea_map_new(tea_State* T);
TEA_FUNC void tea_map_clear(tea_State* T, GCmap* map);
TEA_FUNC TValue* tea_map_set(tea_State* T, GCmap* map, TValue* key);
TEA_FUNC TValue* tea_map_get(GCmap* map, TValue* key);
TEA_FUNC bool tea_map_delete(tea_State* T, GCmap* map, TValue* key);
TEA_FUNC void tea_map_addall(tea_State* T, GCmap* from, GCmap* to);

static TEA_AINLINE bool tea_map_hashable(TValue* value)
{
    return tvisnull(value) || tvisbool(value) || tvisnumber(value) ||
    tvisstr(value);
}

#endif