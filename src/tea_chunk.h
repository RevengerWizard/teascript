#ifndef TEA_CHUNK_H
#define TEA_CHUNK_H

#include "tea_common.h"
#include "tea_predefines.h"
#include "tea_value.h"

typedef enum
{
    #define OPCODE(name, _) OP_##name
    #include "tea_opcodes.h"
    #undef OPCODE
} TeaOpCode;

typedef struct
{
    int count;
    int capacity;
    uint8_t* code;
    
    int* lines;
    
    TeaValueArray constants;
} TeaChunk;

void tea_init_chunk(TeaChunk* chunk);
void tea_free_chunk(TeaState* state, TeaChunk* chunk);
void tea_write_chunk(TeaState* state, TeaChunk* chunk, uint8_t byte, int line);
int tea_add_constant(TeaState* state, TeaChunk* chunk, TeaValue value);

#endif