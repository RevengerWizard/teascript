/*
** tea_gc.h
** Teascript garbage collector
*/

#ifndef TEA_GC_H
#define TEA_GC_H

#include "tea_state.h"

#define GC_HEAP_GROW_FACTOR 2

void teaC_mark_object(TeaState* T, TeaObject* object);
void teaC_mark_value(TeaState* T, TeaValue value);

void teaC_free_objects(TeaState* T);

#endif