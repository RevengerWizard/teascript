#ifndef TEA_DEBUG_H
#define TEA_DEBUG_H

#include "tea_chunk.h"

void tea_disassemble_chunk(TeaChunk* chunk, const char* name);
int tea_disassemble_instruction(TeaChunk* chunk, int offset);

#endif