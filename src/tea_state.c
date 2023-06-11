/*
** tea_state.c
** Teascript global state
*/

#include <stdlib.h>
#include <string.h>

#define tea_state_c
#define TEA_CORE

#include "tea_state.h"
#include "tea_core.h"
#include "tea_vm.h"
#include "tea_string.h"
#include "tea_util.h"
#include "tea_do.h"
#include "tea_gc.h"

static void free_state(TeaState* T)
{
    free(T);
}

static void free_stack(TeaState* T)
{
    TEA_FREE_ARRAY(T, TeaCallInfo, T->base_ci, T->ci_size);
    TEA_FREE_ARRAY(T, TeaValue, T->stack, T->stack_size);
}

static void init_stack(TeaState* T)
{
    T->stack = TEA_ALLOCATE(T, TeaValue, BASE_STACK_SIZE);
    T->stack_size = BASE_STACK_SIZE;
    T->base = T->top = T->stack;
    T->stack_last = T->stack + T->stack_size - 1;
    T->base_ci = TEA_ALLOCATE(T, TeaCallInfo, BASIC_CI_SIZE);
    T->ci_size = BASIC_CI_SIZE;
    T->ci = T->base_ci;
    T->end_ci = T->base_ci + T->ci_size - 1;
    T->nccalls = 0;
    T->open_upvalues = NULL;
}

static void default_panic(TeaState* T)
{
    puts("panic");
}

TEA_API TeaState* tea_open()
{
    TeaState* T = (TeaState*)malloc(sizeof(*T));
    if(T == NULL) 
        return T;
    T->error_jump = NULL;
    T->objects = NULL;
    T->last_module = NULL;
    T->bytes_allocated = 0;
    T->next_gc = 1024 * 1024;
    init_stack(T);
    T->panic = default_panic;
    T->gray_stack = NULL;
    T->gray_count = 0;
    T->gray_capacity = 0;
    T->list_class = NULL;
    T->string_class = NULL;
    T->map_class = NULL;
    T->file_class = NULL;
    T->range_class = NULL;
    tea_table_init(&T->modules);
    tea_table_init(&T->globals);
    tea_table_init(&T->constants);
    tea_table_init(&T->strings);
    T->constructor_string = tea_string_literal(T, "constructor");
    T->repl_string = tea_string_literal(T, "_");
    T->repl = false;
    tea_open_core(T);
    return T;
}

TEA_API void tea_close(TeaState* T)
{
    T->constructor_string = NULL;
    T->repl_string = NULL;
    
    if(T->repl) 
        tea_table_free(T, &T->constants);

    tea_table_free(T, &T->modules);
    tea_table_free(T, &T->globals);
    tea_table_free(T, &T->constants);
    tea_table_free(T, &T->strings);
    free_stack(T);
    tea_gc_free_objects(T);

#if defined(TEA_DEBUG_TRACE_MEMORY) || defined(TEA_DEBUG_FINAL_MEMORY)
    printf("total bytes lost: %zu\n", T->bytes_allocated);
#endif

    free_state(T);
}

TeaObjectClass* tea_state_get_class(TeaState* T, TeaValue value)
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
            default:;
        }
    }
    return NULL;
}

TEA_API TeaInterpretResult tea_interpret(TeaState* T, const char* module_name, const char* source)
{
    TeaObjectString* name = tea_string_new(T, module_name);
    tea_vm_push(T, OBJECT_VAL(name));
    TeaObjectModule* module = tea_obj_new_module(T, name);
    tea_vm_pop(T, 1);

    tea_vm_push(T, OBJECT_VAL(module));
    module->path = tea_util_get_directory(T, (char*)module_name);
    tea_vm_pop(T, 1);
    
    int status = tea_do_protected_compiler(T, module, source);
    if(status != 0)
        return TEA_COMPILE_ERROR;

    return tea_do_pcall(T, T->top[-1], 0);
}