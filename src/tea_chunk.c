// tea_chunk.c
// Teascript chunks

#include "tea_chunk.h"
#include "tea_memory.h"
#include "tea_state.h"
#include "tea_value.h"

void tea_init_chunk(TeaChunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    tea_init_value_array(&chunk->constants);
}

void tea_free_chunk(TeaState* T, TeaChunk* chunk)
{
    TEA_FREE_ARRAY(T, uint8_t, chunk->code, chunk->capacity);
    TEA_FREE_ARRAY(T, int, chunk->lines, chunk->capacity);
    tea_free_value_array(T, &chunk->constants);
    tea_init_chunk(chunk);
}

void tea_write_chunk(TeaState* T, TeaChunk* chunk, uint8_t byte, int line)
{
    if(chunk->capacity < chunk->count + 1)
    {
        int old_capacity = chunk->capacity;
        chunk->capacity = TEA_GROW_CAPACITY(old_capacity);
        chunk->code = TEA_GROW_ARRAY(T, uint8_t, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = TEA_GROW_ARRAY(T, int, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int tea_add_constant(TeaState* T, TeaChunk* chunk, TeaValue value)
{
    tea_push_slot(T, value);
    tea_write_value_array(T, &chunk->constants, value);
    tea_pop_slot(T);

    return chunk->constants.count - 1;
}