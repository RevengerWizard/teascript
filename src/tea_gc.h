/*
** tea_gc.h
** Teascript garbage collector
*/

#ifndef TEA_GC_H
#define TEA_GC_H

#include "tea_state.h"

TEA_FUNC void tea_gc_mark_object(TeaState* T, TeaObject* object);
TEA_FUNC void tea_gc_mark_value(TeaState* T, TeaValue value);

TEA_FUNC void tea_gc_free_objects(TeaState* T);

#endif