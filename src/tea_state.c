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
    (*T->frealloc)(T->ud, T, sizeof(*T), 0);
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
}

static void t_panic(TeaState* T)
{
    fputs("PANIC: unprotected error in call to Teascript API", stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

static void* t_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    (void)ud; 
    (void)osize;
    if(nsize == 0)
    {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, nsize);
}

static void init_opmethods(TeaState* T)
{
    static const char* const tea_state_opmnames[] = {
        "+", "-", "*", "/", "%", "**", 
        "&", "|", "~", "^", "<<", ">>", 
        "<", "<=", ">", ">=", "==", 
        "[]", "tostring"
    };
    for(int i = 0; i < MT_END; i++)
    {
        T->opm_name[i] = tea_str_new(T, tea_state_opmnames[i]);
    }
}

TEA_API TeaState* tea_new_state(TeaAlloc f, void* ud)
{
    TeaState* T;
    f = (f != NULL) ? f : t_alloc;
    T = (TeaState*)((*f)(ud, NULL, 0, sizeof(*T)));
    if(T == NULL) 
        return T;
    T->frealloc = f;
    T->ud = ud;
    T->error_jump = NULL;
    T->nccalls = 0;
    T->objects = NULL;
    T->last_module = NULL;
    T->bytes_allocated = 0;
    T->next_gc = 1024 * 1024;
    T->panic = t_panic;
    T->argc = 0;
    T->argv = NULL;
    T->argf = 0;
    T->repl = false;
    T->open_upvalues = NULL;
    T->gray_stack = NULL;
    T->gray_count = 0;
    T->gray_capacity = 0;
    T->list_class = NULL;
    T->string_class = NULL;
    T->map_class = NULL;
    T->file_class = NULL;
    T->range_class = NULL;
    init_stack(T);
    tea_tab_init(&T->modules);
    tea_tab_init(&T->globals);
    tea_tab_init(&T->constants);
    tea_tab_init(&T->strings);
    T->constructor_string = tea_str_literal(T, "constructor");
    T->repl_string = tea_str_literal(T, "_");
    init_opmethods(T);
    tea_open_core(T);
    return T;
}

TEA_API void tea_close(TeaState* T)
{
    T->constructor_string = NULL;
    T->repl_string = NULL;
    
    if(T->repl) 
        tea_tab_free(T, &T->constants);

    tea_tab_free(T, &T->modules);
    tea_tab_free(T, &T->globals);
    tea_tab_free(T, &T->constants);
    tea_tab_free(T, &T->strings);
    free_stack(T);
    tea_gc_free_objects(T);

#if defined(TEA_DEBUG_TRACE_MEMORY) || defined(TEA_DEBUG_FINAL_MEMORY)
    printf("total bytes lost: %zu\n", T->bytes_allocated);
#endif

    free_state(T);
}

TeaOClass* tea_state_get_class(TeaState* T, TeaValue value)
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
            default: return NULL;
        }
    }
    return NULL;
}

bool tea_state_isclass(TeaState* T, TeaOClass* klass)
{
    return (klass == T->list_class ||
           klass == T->map_class ||
           klass == T->string_class ||
           klass == T->range_class ||
           klass == T->file_class);
}

TEA_API TeaStatus tea_interpret(TeaState* T, const char* module_name, const char* source)
{
    TeaOString* name = tea_str_new(T, module_name);
    tea_vm_push(T, OBJECT_VAL(name));
    TeaOModule* module = tea_obj_new_module(T, name);
    tea_vm_pop(T, 1);

    char c = module_name[0];
    tea_vm_push(T, OBJECT_VAL(module));
    if(c != '<' && c != '?' && c != '=')
    {
        module->path = tea_util_get_directory(T, (char*)module_name);
    }
    tea_vm_pop(T, 1);
    
    int status = tea_do_protected_compiler(T, module, source);
    if(status != TEA_OK)
        return TEA_SYNTAX_ERROR;

    return tea_do_pcall(T, T->top[-1], 0);
}