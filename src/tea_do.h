/*
** tea_do.h
** Stack and Call structure of Teascript
*/

#ifndef TEA_DO_H
#define TEA_DO_H

#include "tea_state.h"

#define teaD_checkstack(T, n) \
    if((char*)T->stack_last - (char*)T->top <= (n)*(int)sizeof(TeaValue)) \
        tea_do_grow_stack(T, n);

#define savestack(T, p)		((char*)(p) - (char*)T->stack)
#define restorestack(T, n)	((TeaValue*)((char*)T->stack + (n)))

#define saveci(T, p)		((char*)(p) - (char*)T->base_ci)
#define restoreci(T, n)		((TeaCallInfo*)((char*)T->base_ci + (n)))

typedef void (*TeaPFunction)(TeaState* T, void* ud);

TEA_FUNC void tea_do_realloc_ci(TeaState* T, int new_size);
TEA_FUNC void tea_do_grow_ci(TeaState* T);

TEA_FUNC void tea_do_realloc_stack(TeaState* T, int new_size);
TEA_FUNC void tea_do_grow_stack(TeaState* T, int needed);

TEA_FUNC bool tea_do_precall(TeaState* T, TeaValue callee, uint8_t arg_count);
TEA_FUNC void tea_do_call(TeaState* T, TeaValue func, int arg_count);
TEA_FUNC int tea_do_pcall(TeaState* T, TeaValue func, int arg_count);

TEA_FUNC void tea_do_throw(TeaState* T, int code);

TEA_FUNC int tea_do_runprotected(TeaState* T, TeaPFunction f, void* ud);

TEA_FUNC int tea_do_protected_compiler(TeaState* T, TeaOModule* module, const char* source);

#endif