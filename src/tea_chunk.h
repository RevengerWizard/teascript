/*
** tea_chunk.h
** Teascript chunks
*/

#ifndef TEA_CHUNK_H
#define TEA_CHUNK_H

#include "tea_def.h"
#include "tea_value.h"

typedef enum
{
    #define OPCODE(name, _) OP_##name
    #include "tea_opcodes.h"
    #undef OPCODE
} TeaOpCode;

typedef struct
{
    int offset;
    int line;
} TeaLineStart;

typedef struct
{
    int count;
    int capacity;
    uint8_t* code;
    TeaValueArray constants;
    int line_count;
    int line_capacity;
    TeaLineStart* lines;
} TeaChunk;

void teaK_init(TeaChunk* chunk);
void teaK_free(TeaState* T, TeaChunk* chunk);
void teaK_write(TeaState* T, TeaChunk* chunk, uint8_t byte, int line);
int teaK_add_constant(TeaState* T, TeaChunk* chunk, TeaValue value);
int teaK_getline(TeaChunk* chunk, int instruction);

#endif