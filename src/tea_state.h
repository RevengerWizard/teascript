/*
** tea_state.h
** State and stack handling
*/

#ifndef _TEA_STATE_H
#define _TEA_STATE_H

#include "tea.h"

#include "tea_def.h"
#include "tea_obj.h"

/* Extra stack space to handle special method calls and other extras */
#define TEA_STACK_EXTRA 5
#define TEA_STACK_START (TEA_MIN_STACK * 2)
#define TEA_CI_MIN 8

#define incr_top(T) \
    (++T->top >= T->stack_max && (tea_state_growstack1(T), 0))

#define stack_save(T, p) ((char*)(p) - (char*)T->stack)
#define stack_restore(T, n)  ((TValue*)((char*)T->stack + (n)))

#define ci_save(T, p)        ((char*)(p) - (char*)T->ci_base)
#define ci_restore(T, n)     ((CallInfo*)((char*)T->ci_base + (n)))

TEA_FUNC void tea_state_growstack(tea_State* T, int needed);
TEA_FUNC void tea_state_growstack1(tea_State* T);
TEA_FUNC void tea_state_reallocci(tea_State* T, int new_size);
TEA_FUNC void tea_state_growci(tea_State* T);

static TEA_AINLINE void tea_state_checkstack(tea_State* T, int need)
{
    if(((char*)T->stack_max - (char*)T->top) <= 
       (ptrdiff_t)(need)*(ptrdiff_t)sizeof(TValue))
        tea_state_growstack(T, need);
}

#endif