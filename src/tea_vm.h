/*
** tea_vm.h
** Teascript virtual machine
*/

#ifndef _TEA_VM_H
#define _TEA_VM_H

#include "tea_state.h"
#include "tea_err.h"

/* Entry points of VM */
TEA_FUNC void tea_vm_call(tea_State* T, TValue* func, int arg_count);
TEA_FUNC int tea_vm_pcall(tea_State* T, tea_CPFunction func, void* u, ptrdiff_t old_top);

#endif