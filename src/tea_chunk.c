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
    FREE_ARRAY(T, uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(T, int, chunk->lines, chunk->capacity);
    tea_free_value_array(T, &chunk->constants);
    tea_init_chunk(chunk);
}

void tea_write_chunk(TeaState* T, TeaChunk* chunk, uint8_t byte, int line)
{
    if(chunk->capacity < chunk->count + 1)
    {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(T, uint8_t, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(T, int, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int tea_add_constant(TeaState* T, TeaChunk* chunk, TeaValue value)
{
    tea_push_root(T, value);
    tea_write_value_array(T, &chunk->constants, value);
    tea_pop_root(T);

    return chunk->constants.count - 1;
}