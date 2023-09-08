/*
** tea_map.h
** Teascript map implementation
*/

#ifndef TEA_MAP_H
#define TEA_MAP_H

#include "tea_object.h"

TEA_FUNC TeaObjectMap* tea_map_new(TeaState* T);

TEA_FUNC void tea_map_clear(TeaState* T, TeaObjectMap* map);
TEA_FUNC bool tea_map_set(TeaState* T, TeaObjectMap* map, TeaValue key, TeaValue value);
TEA_FUNC bool tea_map_get(TeaObjectMap* map, TeaValue key, TeaValue* value);
TEA_FUNC bool tea_map_delete(TeaState* T, TeaObjectMap* map, TeaValue key);
TEA_FUNC void tea_map_add_all(TeaState* T, TeaObjectMap* from, TeaObjectMap* to);

static inline bool tea_map_validkey(TeaValue value)
{
    return IS_NULL(value) || IS_BOOL(value) || IS_NUMBER(value) ||
    IS_STRING(value);
}

#endif