/*
** tea_state.c
** Teascript global state
*/

#define tea_state_c
#define TEA_CORE

#include <stdlib.h>
#include <string.h>

#include "tealib.h"

#include "tea_def.h"
#include "tea_state.h"
#include "tea_vm.h"
#include "tea_str.h"
#include "tea_import.h"
#include "tea_err.h"
#include "tea_gc.h"
#include "tea_tab.h"
#include "tea_buf.h"

/* -- Stack handling -------------------------------------------------- */

static void stack_free(tea_State* T)
{
    tea_mem_freevec(T, CallInfo, T->ci_base, T->ci_size);   /* Free CallInfo array */
    tea_mem_freevec(T, TValue, T->stack, T->stack_size);    /* Free stack array */
}

/* Allocate basic stack for new state */
static void stack_init(tea_State* T)
{
    /* Initialize stack array */
    T->stack = tea_mem_new(T, TValue, TEA_STACK_START + TEA_STACK_EXTRA);
    T->stack_size = TEA_STACK_START + TEA_STACK_EXTRA;
    T->top = T->stack;
    T->stack_max = T->stack + (T->stack_size - TEA_STACK_EXTRA) - 1;
    /* Initialize CallInfo array */
    T->ci_base = tea_mem_new(T, CallInfo, TEA_CI_MIN);
    T->ci_size = TEA_CI_MIN;
    T->ci = T->ci_base;
    T->base = T->ci->base = T->top;
    T->ci_end = T->ci_base + T->ci_size;
    /* Initialize first ci */
    T->ci->func = NULL;
    T->ci->cfunc = NULL;
    T->ci->state = CIST_C;
    setnullV(T->top++);
}

static const char* const opmnames[] = {
#define MMSTR(_, name) #name,
    MMDEF(MMSTR)
#undef MMSTR
};

static void state_init_mms(tea_State* T)
{
    for(int i = 0; i < MM__MAX; i++)
    {
        T->opm_name[i] = tea_str_new(T, opmnames[i]);
    }
}

/* Adjust pointers in state */
static void stack_correct(tea_State* T, TValue* old_stack)
{
    T->top = (T->top - old_stack) + T->stack;

    for(GCupvalue* upvalue = T->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        upvalue->location = (upvalue->location - old_stack) + T->stack;
    }

    for(CallInfo* ci = T->ci_base; ci <= T->ci; ci++)
    {
        ci->base = (ci->base - old_stack) + T->stack;
    }

    T->base = (T->base - old_stack) + T->stack;
}

/* Resize stack slots */
static void stack_realloc(tea_State* T, int new_size)
{
	TValue* old_stack = T->stack;
	T->stack = tea_mem_reallocvec(T, TValue, T->stack, T->stack_size, new_size);
	T->stack_size = new_size;
    T->stack_max = T->stack + new_size - 1 - TEA_STACK_EXTRA;
    if(old_stack != T->stack)
    {
        stack_correct(T, old_stack);
    }
}

void tea_state_reallocci(tea_State* T, int new_size)
{
    CallInfo* old_ci = T->ci_base;
    T->ci_base = tea_mem_reallocvec(T, CallInfo, T->ci_base, T->ci_size, new_size);
    T->ci_size = new_size;
    T->ci = (T->ci - old_ci) + T->ci_base;
    T->ci_end = T->ci_base + T->ci_size;
}

void tea_state_growci(tea_State* T)
{
    if(T->ci + 1 == T->ci_end)
    {
        tea_state_reallocci(T, T->ci_size * 2);
    }
    if(T->ci_size > TEA_MAX_CALLS)
    {
        tea_err_run(T, TEA_ERR_STKOV);
    }
}

/* Try to grow stack */
void tea_state_growstack(tea_State* T, int needed)
{
	if(needed <= T->stack_size)
        stack_realloc(T, 2 * T->stack_size);
    else
        stack_realloc(T, T->stack_size + needed + TEA_STACK_EXTRA);
}

void tea_state_growstack1(tea_State* T)
{
    tea_state_growstack(T, 1);
}

/* -- Default allocator and panic function -------------------------------- */

static void panic(tea_State* T)
{
    fputs("PANIC: unprotected error in call to Teascript API", stderr);
    fputc('\n', stderr);
    fflush(stderr);
}

static void* mem_alloc(void* ud, void* ptr, size_t osize, size_t nsize)
{
    UNUSED(ud);
    UNUSED(osize);
    if(nsize == 0)
    {
        free(ptr);
        return NULL;
    }

    return realloc(ptr, nsize);
}

/* -- State handling -------------------------------------------------- */

static void state_free(tea_State* T)
{
    T->allocf(T->allocd, T, sizeof(*T), 0);
}

TEA_API tea_State* tea_new_state(tea_Alloc allocf, void* ud)
{
    tea_State* T;
    allocf = (allocf != NULL) ? allocf : mem_alloc;
    T = (tea_State*)allocf(ud, NULL, 0, sizeof(*T));
    if(T == NULL)
        return T;
    T->allocf = allocf;
    T->allocd = ud;
    T->error_jump = NULL;
    T->parser = NULL;
    T->nccalls = 0;
    T->last_module = NULL;
    T->gc.objects = NULL;
    T->gc.bytes_allocated = 0;
    T->gc.gray_stack = NULL;
    T->gc.gray_count = 0;
    T->gc.gray_size = 0;
    T->gc.next_gc = 1024 * 1024;
    T->panic = panic;
    T->argc = 0;
    T->argv = NULL;
    T->argf = 0;
    T->repl = false;
    T->open_upvalues = NULL;
    T->number_class = NULL;
    T->bool_class = NULL;
    T->list_class = NULL;
    T->string_class = NULL;
    T->map_class = NULL;
    T->file_class = NULL;
    T->range_class = NULL;
    stack_init(T);
    tea_buf_init(&T->tmpbuf);
    tea_tab_init(&T->modules);
    tea_tab_init(&T->globals);
    tea_tab_init(&T->constants);
    tea_tab_init(&T->strings);
    T->constructor_string = tea_str_newlit(T, "constructor");
    T->repl_string = tea_str_newlit(T, "_");
    T->memerr = tea_str_new(T, err2msg(TEA_ERR_MEM));
    state_init_mms(T);
    tea_open_core(T);
    return T;
}

TEA_API void tea_close(tea_State* T)
{
    if(T->repl)
        tea_tab_free(T, &T->constants);

    tea_buf_free(T, &T->tmpbuf);
    tea_tab_free(T, &T->modules);
    tea_tab_free(T, &T->globals);
    tea_tab_free(T, &T->constants);
    tea_tab_free(T, &T->strings);
    stack_free(T);
    tea_gc_freeall(T);

#if defined(TEA_DEBUG_TRACE_MEMORY) || defined(TEA_DEBUG_FINAL_MEMORY)
    printf("total bytes lost: %llu\n", T->gc.bytes_allocated);
#endif

    state_free(T);
}

GCclass* tea_state_getclass(tea_State* T, TValue* value)
{
    switch(itype(value))
    {
        case TEA_TNUMBER:
            return T->number_class;
        case TEA_TBOOL:
            return T->bool_class;
        case TEA_TINSTANCE:
            return instanceV(value)->klass;
        case TEA_TLIST: 
            return T->list_class;
        case TEA_TMAP: 
            return T->map_class;
        case TEA_TSTRING: 
            return T->string_class;
        case TEA_TRANGE: 
            return T->range_class;
        case TEA_TFILE: 
            return T->file_class;
        default: 
            return NULL;
    }
    return NULL;
}

bool tea_state_isclass(tea_State* T, GCclass* klass)
{
    return (klass == T->number_class ||
            klass == T->bool_class ||
            klass == T->list_class ||
            klass == T->map_class ||
            klass == T->string_class ||
            klass == T->range_class ||
            klass == T->file_class);
}