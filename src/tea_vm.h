// tea_vm.h
// Teascript virtual machine

#ifndef TEA_VM_H
#define TEA_VM_H

#include "tea_state.h"

void teaV_runtime_error(TeaState* T, const char* format, ...);
void teaV_run(TeaState* T);

static inline void teaV_push(TeaState* T, TeaValue value)
{
    *T->top = value;
    T->top++;
}

static inline TeaValue teaV_pop(TeaState* T, int n)
{
    T->top -= n;
    return *T->top;
}

static inline TeaValue teaV_peek(TeaState* T, int distance)
{
    return T->top[-1 - (distance)];
}

#endif