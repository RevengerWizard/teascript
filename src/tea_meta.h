/*
** tea_meta.h
** Method handling
*/

#ifndef _TEA_META_H
#define _TEA_META_H

#include "tea_obj.h"

TEA_FUNC void tea_meta_init(tea_State* T);
TEA_FUNC TValue* tea_meta_lookup(tea_State* T, cTValue* o, MMS mm);

TEA_FUNC GCclass* tea_meta_getclass(tea_State* T, cTValue* value);
TEA_FUNC bool tea_meta_isclass(tea_State* T, GCclass* klass);

TEA_FUNC cTValue* tea_meta_getattr(tea_State* T, GCstr* name, TValue* obj);
TEA_FUNC cTValue* tea_meta_setattr(tea_State* T, GCstr* name, TValue* obj, TValue* item);
TEA_FUNC cTValue* tea_meta_getindex(tea_State* T, TValue* obj, TValue* index_value);
TEA_FUNC cTValue* tea_meta_setindex(tea_State* T, TValue* obj, TValue* index_value, TValue* item_value);

#endif