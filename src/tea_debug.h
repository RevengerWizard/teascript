/*
** tea_debug.h
** Teascript debug functions
*/

#ifndef TEA_DEBUG_H
#define TEA_DEBUG_H

#include "tea_chunk.h"

void tea_debug_print_value(TeaValue value);
void tea_debug_chunk(TeaState* T, TeaChunk* chunk, const char* name);
void tea_debug_stack(TeaState* T);
int tea_debug_instruction(TeaState* T, TeaChunk* chunk, int offset);

#endif