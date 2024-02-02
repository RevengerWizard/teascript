/*
** tea_vm.h
** Teascript virtual machine
*/

#ifndef _TEA_VM_H
#define _TEA_VM_H

#include "tea_state.h"
#include "tea_err.h"

TEA_FUNC void tea_vm_call(tea_State* T, Value func, int arg_count);
TEA_FUNC int tea_vm_pcall(tea_State* T, tea_CPFunction func, void* u, ptrdiff_t old_top);

static TEA_INLINE void tea_vm_push(tea_State* T, Value value)
{
    *T->top = value;
    T->top++;
}

static TEA_INLINE Value tea_vm_pop(tea_State* T, int n)
{
    T->top -= n;
    return *T->top;
}

static TEA_INLINE Value tea_vm_peek(tea_State* T, int distance)
{
    return T->top[-1 - distance];
}

#endif