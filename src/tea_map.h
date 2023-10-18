/*
** tea_map.h
** Teascript map implementation
*/

#ifndef TEA_MAP_H
#define TEA_MAP_H

#include "tea_object.h"

TEA_FUNC TeaOMap* tea_map_new(TeaState* T);

TEA_FUNC void tea_map_clear(TeaState* T, TeaOMap* map);
TEA_FUNC bool tea_map_set(TeaState* T, TeaOMap* map, TeaValue key, TeaValue value);
TEA_FUNC bool tea_map_get(TeaOMap* map, TeaValue key, TeaValue* value);
TEA_FUNC bool tea_map_delete(TeaState* T, TeaOMap* map, TeaValue key);
TEA_FUNC void tea_map_addall(TeaState* T, TeaOMap* from, TeaOMap* to);

static inline bool tea_map_hashable(TeaValue value)
{
    return IS_NULL(value) || IS_BOOL(value) || IS_NUMBER(value) ||
    IS_STRING(value);
}

#endif