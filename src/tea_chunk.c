/*
** tea_chunk.c
** Teascript chunks
*/

#define tea_chunk_c
#define TEA_CORE

#include "tea_chunk.h"
#include "tea_memory.h"
#include "tea_state.h"
#include "tea_value.h"
#include "tea_vm.h"

void tea_chunk_init(TeaChunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->line_count = 0;
    chunk->line_capacity = 0;
    chunk->lines = NULL;
    tea_init_value_array(&chunk->constants);
}

void tea_chunk_free(TeaState* T, TeaChunk* chunk)
{
    TEA_FREE_ARRAY(T, uint8_t, chunk->code, chunk->capacity);
    TEA_FREE_ARRAY(T, TeaLineStart, chunk->lines, chunk->line_capacity);
    tea_free_value_array(T, &chunk->constants);
    tea_chunk_init(chunk);
}

void tea_chunk_write(TeaState* T, TeaChunk* chunk, uint8_t byte, int line)
{
    if(chunk->capacity < chunk->count + 1)
    {
        int old_capacity = chunk->capacity;
        chunk->capacity = TEA_GROW_CAPACITY(old_capacity);
        chunk->code = TEA_GROW_ARRAY(T, uint8_t, chunk->code, old_capacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    chunk->count++;

    /* See if we're still on the same line */
    if(chunk->line_count > 0 && chunk->lines[chunk->line_count - 1].line == line)
    {
        return;
    }

    /* Append a new LineStart */
    if(chunk->line_capacity < chunk->line_count + 1)
    {
        int old_capacity = chunk->line_capacity;
        chunk->line_capacity = TEA_GROW_CAPACITY(old_capacity);
        chunk->lines = TEA_GROW_ARRAY(T, TeaLineStart, chunk->lines, old_capacity, chunk->line_capacity);
    }

    TeaLineStart* line_start = &chunk->lines[chunk->line_count++];
    line_start->offset = chunk->count - 1;
    line_start->line = line;
}

int tea_chunk_add_constant(TeaState* T, TeaChunk* chunk, TeaValue value)
{
    tea_vm_push(T, value);
    tea_write_value_array(T, &chunk->constants, value);
    tea_vm_pop(T, 1);

    return chunk->constants.count - 1;
}

int tea_chunk_getline(TeaChunk* chunk, int instruction)
{
    int start = 0;
    int end = chunk->line_count - 1;

    while(true)
    {
        int mid = (start + end) / 2;
        TeaLineStart* line = &chunk->lines[mid];
        if(instruction < line->offset)
        {
            end = mid - 1;
        }
        else if(mid == chunk->line_count - 1 || instruction < chunk->lines[mid + 1].offset)
        {
            return line->line;
        }
        else
        {
            start = mid + 1;
        }
    }
}