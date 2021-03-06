#include "tea_chunk.h"
#include "tea_memory.h"
#include "tea_vm.h"
#include "tea_value.h"

void tea_init_chunk(TeaChunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;

    chunk->lines = NULL;

    tea_init_value_array(&chunk->constants);
}

void tea_free_chunk(TeaState* state, TeaChunk* chunk)
{
    FREE_ARRAY(state, uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(state, int, chunk->lines, chunk->capacity);
    tea_free_value_array(state, &chunk->constants);
    tea_init_chunk(chunk);
}

void tea_write_chunk(TeaState* state, TeaChunk* chunk, uint8_t byte, int line)
{
    if(chunk->capacity < chunk->count + 1)
    {
        int old_capacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(old_capacity);
        chunk->code = GROW_ARRAY(state, uint8_t, chunk->code, old_capacity, chunk->capacity);
        chunk->lines = GROW_ARRAY(state, int, chunk->lines, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int tea_add_constant(TeaState* state, TeaChunk* chunk, TeaValue value)
{
    tea_push_root(state, value);
    tea_write_value_array(state, &chunk->constants, value);
    tea_pop_root(state);

    return chunk->constants.count - 1;
}