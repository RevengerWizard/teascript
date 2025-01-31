/*
** tea_state.c
** Teascript global state
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define tea_state_c
#define TEA_CORE

#include "tealib.h"

#include "tea_def.h"
#include "tea_state.h"
#include "tea_str.h"
#include "tea_err.h"
#include "tea_gc.h"
#include "tea_tab.h"
#include "tea_buf.h"
#include "tea_meta.h"
#include "tea_lex.h"
#include "tea_map.h"
#include "tea_func.h"
#include "tea_import.h"

/* -- Stack handling -------------------------------------------------- */

/* Allocate basic stack for new state */
static void stack_init(tea_State* T)
{
    /* Initialize stack array */
    T->stack = tea_mem_newvec(T, TValue, TEA_STACK_START + TEA_STACK_EXTRA);
    T->stack_size = TEA_STACK_START + TEA_STACK_EXTRA;
    T->top = T->stack;
    T->stack_max = T->stack + (T->stack_size - TEA_STACK_EXTRA) - 1;
    /* Initialize CallInfo array */
    T->ci_base = tea_mem_newvec(T, CallInfo, TEA_CI_MIN);
    T->ci_size = TEA_CI_MIN;
    T->ci = T->ci_base;
    /* Initialize first ci */
    T->ci->state = CIST_C;
    T->ci->func = NULL;
    T->base = T->ci->base = T->top;
    T->ci_end = T->ci_base + T->ci_size;
}

/* Resize stack slots and adjust pointers in state */
static void stack_resize(tea_State* T, int new_size)
{
	TValue* old_stack = T->stack;
	T->stack = tea_mem_reallocvec(T, TValue, T->stack, T->stack_size, new_size);
	T->stack_size = new_size;
    T->stack_max = T->stack + new_size - 1 - TEA_STACK_EXTRA;
    T->top = (T->top - old_stack) + T->stack;
    for(GCupval* uv = T->open_upvalues; uv != NULL; uv = uv->next)
    {
        uv->location = (uv->location - old_stack) + T->stack;
    }
    for(CallInfo* ci = T->ci_base; ci <= T->ci; ci++)
    {
        ci->base = (ci->base - old_stack) + T->stack;
    }
    T->base = (T->base - old_stack) + T->stack;
}

void tea_state_reallocci(tea_State* T, int new_size)
{
    CallInfo* old_ci = T->ci_base;
    T->ci_base = tea_mem_reallocvec(T, CallInfo, T->ci_base, T->ci_size, new_size);
    T->ci_size = new_size;
    T->ci = (T->ci - old_ci) + T->ci_base;
    T->ci_end = T->ci_base + T->ci_size;
}

CallInfo* tea_state_growci(tea_State* T)
{
    if(T->ci + 1 == T->ci_end)
    {
        tea_state_reallocci(T, T->ci_size * 2);
    }
    if(T->ci_size > TEA_MAX_CALLS)
    {
        tea_err_stkov(T);
    }
    return ++T->ci;
}

/* Relimit stack after error, in case the limit was overdrawn */
void tea_state_relimitstack(tea_State* T)
{
    T->stack_max = T->stack + T->stack_size - 1;
    if(T->ci_size > TEA_MAX_CALLS)
    {
        int inuse = T->ci - T->ci_base;
        if(inuse + 1 < TEA_MAX_CALLS)
        {
            tea_state_reallocci(T, TEA_MAX_CALLS);
        }
    }
}

/* Try to grow stack */
void tea_state_growstack(tea_State* T, int need)
{
    if(need <= T->stack_size)
    {
        stack_resize(T, 2 * T->stack_size);
    }
    else
    {
        stack_resize(T, T->stack_size + need + TEA_STACK_EXTRA);
    }
}

void tea_state_growstack1(tea_State* T)
{
    tea_state_growstack(T, 1);
}

/* -- Default allocator and panic function -------------------------------- */

static void panic(tea_State* T)
{
    const char* s = tea_to_string(T, -1);
    fputs("PANIC: unprotected error in call to Teascript API (", stderr);
    fputs(s ? s : "?", stderr);
    fputc(')', stderr); fputc('\n', stderr);
    fflush(stderr);
    tea_pop(T, 1);
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
    else
    {
        return realloc(ptr, nsize);
    }
}

/* -- State handling -------------------------------------------------- */

/* Open parts that may cause memory-allocation errors */
static void cpteaopen(tea_State* T, void* ud)
{
    UNUSED(ud);
    stack_init(T);
    setmapV(T, registry(T), tea_map_new(T));
    tea_str_init(T);
    tea_meta_init(T);
    tea_lex_init(T);
    fix_string(tea_err_str(T, TEA_ERR_MEM));
}

static void state_close(tea_State* T)
{
    tea_buf_free(T, &T->tmpbuf);
    tea_buf_free(T, &T->strbuf);
    tea_tab_free(T, &T->modules);
    tea_tab_free(T, &T->globals);
    tea_tab_free(T, &T->constants);
    tea_gc_freeall(T);
    tea_imp_freehandle(T);  /* Close pending library handles */
    tea_str_freetab(T);
    tea_mem_freevec(T, CallInfo, T->ci_base, T->ci_size);   /* Free CallInfo array */
    tea_mem_freevec(T, TValue, T->stack, T->stack_size);    /* Free stack array */
    tea_assertT(T->str.num == 0, "leaked %d strings", T->str.num);
    tea_assertT(T->gc.total == 0, "memory leak of %llu bytes", T->gc.total);
    T->allocf(T->allocd, T, sizeof(*T), 0); /* Free the state */
}

TEA_API tea_State* tea_new_state(tea_Alloc allocf, void* ud)
{
    tea_State* T;
    allocf = (allocf != NULL) ? allocf : mem_alloc;
    T = (tea_State*)allocf(ud, NULL, 0, sizeof(*T));
    if(T == NULL)
        return NULL;
    memset(T, 0, sizeof(*T));
    T->allocf = allocf;
    T->allocd = ud;
    T->gc.next_gc = 1024 * 1024;
    T->panic = panic;
    T->strempty.obj.gct = TEA_TSTR;
    T->strempty.obj.marked = TEA_GC_FIXED;
    tea_buf_init(&T->tmpbuf);
    tea_buf_init(&T->strbuf);
    tea_tab_init(&T->modules);
    tea_tab_init(&T->globals);
    tea_tab_init(&T->constants);
    setnilV(&T->nilval);
    if(tea_err_protected(T, cpteaopen, NULL) != TEA_OK)
    {
        /* Memory allocation error: free partial state */
        state_close(T);
        return NULL;
    }
    tea_open_base(T);
    return T;
}

static void cpfinalize(tea_State* T, void* ud)
{
    UNUSED(ud);
    tea_gc_finalize_udata(T);
}

TEA_API void tea_close(tea_State* T)
{
    tea_func_closeuv(T, T->stack);
    tea_gc_separateudata(T);
    do
    {
        T->ci = T->ci_base;
        T->base = T->top = T->ci->base;
        T->nccalls = 0;
    }
    while(tea_err_protected(T, cpfinalize, NULL) != 0);
    tea_assertT(T->gc.mmudata == NULL, "lost userdata finalizers");
    state_close(T);
}