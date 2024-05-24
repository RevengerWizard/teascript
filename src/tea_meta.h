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

#endif