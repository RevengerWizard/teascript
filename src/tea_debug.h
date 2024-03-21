/*
** tea_debug.h
** Teascript debug functions
*/

#ifndef _TEA_DEBUG_H
#define _TEA_DEBUG_H

#include "tea_obj.h"

void tea_debug_value(TValue* value);
void tea_debug_chunk(tea_State* T, GCproto* f, const char* name);
void tea_debug_stack(tea_State* T);
int tea_debug_instruction(tea_State* T, GCproto* f, int offset);

#endif