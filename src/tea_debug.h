// tea_debug.h
// Teascript debug functions

#ifndef TEA_DEBUG_H
#define TEA_DEBUG_H

#include "tea_chunk.h"

void tea_disassemble_chunk(TeaState* T, TeaChunk* chunk, const char* name);
int tea_disassemble_instruction(TeaState* T, TeaChunk* chunk, int offset);

#endif