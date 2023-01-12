// tea_vm.h
// Teascript virtual machine

#ifndef TEA_VM_H
#define TEA_VM_H

#include "tea_state.h"

void tea_runtime_error(TeaState* T, const char* format, ...);
TeaInterpretResult tea_interpret_module(TeaState* T, const char* module_name, const char* source);

#endif