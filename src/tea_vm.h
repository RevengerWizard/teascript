/*
** tea_vm.h
** Teascript virtual machine
*/

#ifndef TEA_VM_H
#define TEA_VM_H

#include "tea_state.h"

TEA_FUNC void tea_vm_concat(TeaState* T);
TEA_FUNC void tea_vm_error(TeaState* T, const char* format, ...);
TEA_FUNC void tea_vm_run(TeaState* T);

static inline void tea_vm_push(TeaState* T, TeaValue value)
{
    *T->top = value;
    T->top++;
}

static inline TeaValue tea_vm_pop(TeaState* T, int n)
{
    T->top -= n;
    return *T->top;
}

static inline TeaValue tea_vm_peek(TeaState* T, int distance)
{
    return T->top[-1 - (distance)];
}

#endif