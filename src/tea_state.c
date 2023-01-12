// tea_state.c
// Teascript global state

#include <stdlib.h>
#include <string.h>

#include "tea_state.h"
#include "tea_compiler.h"
#include "tea_core.h"
#include "tea_vm.h"

TEA_API TeaState* tea_open()
{
    TeaState* T = (TeaState*)malloc(sizeof(*T));
    if(T == NULL) return T;
    memset(T, 0, sizeof(TeaState));
    T->objects = NULL;
    T->last_module = NULL;
    T->bytes_allocated = 0;
    T->next_gc = 1024 * 1024;
    T->slot = T->stack;
    T->info_count = 0;
    T->top = 0;
    T->gray_stack = NULL;
    T->gray_count = 0;
    T->gray_capacity = 0;
    T->list_class = NULL;
    T->string_class = NULL;
    T->map_class = NULL;
    T->file_class = NULL;
    T->range_class = NULL;
    tea_init_table(&T->modules);
    tea_init_table(&T->globals);
    tea_init_table(&T->strings);
    T->constructor_string = tea_copy_string(T, "constructor", 11);
    tea_open_core(T);
    return T;
}

TEA_API void tea_close(TeaState* T)
{
    tea_free_table(T, &T->modules);
    tea_free_table(T, &T->globals);
    tea_free_table(T, &T->strings);
    T->constructor_string = NULL;
    tea_free_objects(T);

    printf("total bytes lost: %zu\n", T->bytes_allocated);
#ifdef DEBUG_TRACE_MEMORY
#endif

    free(T);
}

TeaObjectClass* tea_get_class(TeaState* T, TeaValue value)
{
    if(IS_OBJECT(value))
    {
        switch(OBJECT_TYPE(value))
        {
            case OBJ_LIST: return T->list_class;
            case OBJ_MAP: return T->map_class;
            case OBJ_STRING: return T->string_class;
            case OBJ_RANGE: return T->range_class;
            case OBJ_FILE: return T->file_class;
        }
    }
    return NULL;
}

TEA_API TeaInterpretResult tea_interpret(TeaState* T, const char* module_name, const char* source)
{
    return tea_interpret_module(T, module_name, source);
}

TEA_API TeaInterpretResult tea_call(TeaState* T, int n)
{
    TeaValue native = T->slot[T->top - n - 1];

    TeaStackInfo* info = &T->infos[T->info_count++];
    info->slot = T->slot; // Save the start of last slot
    info->top = T->top - n - 1;    // Save top of the last slot
    
    T->slot = T->slot - n + T->top;     // Offset slot to new position (first argument of function)
    T->top = n;

    if(IS_NATIVE(native)) 
    { 
        AS_NATIVE(native)->fn(T);
    }

    TeaValue ret = T->slot[T->top - 1];
    info = &T->infos[--T->info_count];
    T->slot = info->slot;     // Offset slot back to last origin
    T->top = info->top;    // Get last top of origin slot

    T->slot[T->top] = ret;
    T->top++;
}