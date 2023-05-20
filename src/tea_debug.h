// tea_debug.h
// Teascript debug functions

#ifndef TEA_DEBUG_H
#define TEA_DEBUG_H

#include "tea_chunk.h"

void teaG_print_value(TeaValue value);
void teaG_dump_chunk(TeaState* T, TeaChunk* chunk, const char* name);
void teaG_dump_stack(TeaState* T);
int teaG_dump_instruction(TeaState* T, TeaChunk* chunk, int offset);

#endif