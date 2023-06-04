/*
** tea_map.h
** Teascript map implementation
*/

#ifndef TEA_MAP_H
#define TEA_MAP_H

#include "tea_object.h"

TeaObjectMap* tea_map_new(TeaState* T);

void tea_map_clear(TeaState* T, TeaObjectMap* map);
bool tea_map_set(TeaState* T, TeaObjectMap* map, TeaValue key, TeaValue value);
bool tea_map_get(TeaObjectMap* map, TeaValue key, TeaValue* value);
bool tea_map_delete(TeaState* T, TeaObjectMap* map, TeaValue key);
void tea_map_add_all(TeaState* T, TeaObjectMap* from, TeaObjectMap* to);

static inline bool tea_map_validkey(TeaValue value)
{
    return IS_NULL(value) || IS_BOOL(value) || IS_NUMBER(value) ||
    IS_STRING(value);
}

#endif