#ifndef TEA_CHUNK_H
#define TEA_CHUNK_H

#include "tea_common.h"
#include "tea_predefines.h"
#include "vm/tea_value.h"

typedef enum
{
    #define OPCODE(name) OP_##name
    #include "vm/tea_opcodes.h"
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

void tea_init_chunk(TeaChunk* chunk);
void tea_free_chunk(TeaState* state, TeaChunk* chunk);
void tea_write_chunk(TeaState* state, TeaChunk* chunk, uint8_t byte, int line);
void tea_write_constant(TeaChunk* chunk, TeaValue value, int line);
int tea_add_constant(TeaState* state, TeaChunk* chunk, TeaValue value);

int tea_get_line(TeaChunk* chunk, int instruction);

#endif