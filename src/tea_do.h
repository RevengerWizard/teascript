/*
** tea_do.h
** Stack and Call structure of Teascript
*/

#ifndef TEA_DO_H
#define TEA_DO_H

#include "tea_state.h"

#define teaD_checkstack(T, n) \
    if((char*)T->stack_last - (char*)T->top <= (n)*(int)sizeof(TeaValue)) \
        teaD_grow_stack(T, n);

#define savestack(T, p)		((char*)(p) - (char*)T->stack)
#define restorestack(T, n)	((TeaValue*)((char*)T->stack + (n)))

#define saveci(T, p)		((char*)(p) - (char*)T->base_ci)
#define restoreci(T, n)		((TeaCallInfo*)((char*)T->base_ci + (n)))

typedef void (*TeaPFunction)(TeaState* T, void* ud);

void teaD_realloc_ci(TeaState* T, int new_size);
void teaD_grow_ci(TeaState* T);

void teaD_realloc_stack(TeaState* T, int new_size);
void teaD_grow_stack(TeaState* T, int needed);

void teaD_precall(TeaState* T, TeaValue callee, uint8_t arg_count);
void teaD_call(TeaState* T, TeaValue func, int arg_count);
int teaD_pcall(TeaState* T, TeaValue func, int arg_count);

void teaD_throw(TeaState* T, int code);

int teaD_runprotected(TeaState* T, TeaPFunction f, void* ud);

int teaD_protected_compiler(TeaState* T, TeaObjectModule* module, const char* source);

#endif