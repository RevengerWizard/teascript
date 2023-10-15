/*
** tea_memory.c
** Teascript memory functions
*/

#include <stdlib.h>

#define tea_memory_c
#define TEA_CORE

#include "tea.h"

#include "tea_def.h"
#include "tea_memory.h"
#include "tea_state.h"
#include "tea_array.h"
#include "tea_do.h"
#include "tea_gc.h"

void* tea_mem_realloc(TeaState* T, void* pointer, size_t old_size, size_t new_size)
{
    T->bytes_allocated += new_size - old_size;

#ifdef TEA_DEBUG_TRACE_MEMORY
    printf("total bytes allocated: %zu\nnew allocation: %zu\nold allocation: %zu\n\n", T->bytes_allocated, new_size, old_size);
#endif

    if(new_size > old_size)
    {
#ifdef TEA_DEBUG_STRESS_GC
        tea_gc_collect(T);
#endif

        if(T->bytes_allocated > T->next_gc)
        {
            tea_gc_collect(T);
        }
    }

    void* block = (*T->frealloc)(T->ud, pointer, old_size, new_size);

    if(block == NULL && new_size > 0)
    {
        puts(T->memerr->chars);
        exit(1);
    }

    return block;
}