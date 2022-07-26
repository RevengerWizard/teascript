/* 
** tea_vm.h
** Teascript virtual machine
*/ 

#ifndef TEA_VM_H
#define TEA_VM_H

#include "tea_predefines.h"
#include "tea_state.h"
#include "tea_object.h"
#include "tea_table.h"
#include "tea_value.h"

typedef struct TeaVM
{
    TeaState* state;

    TeaTable modules;
    TeaTable globals;
    TeaTable strings;

    TeaObjectModule* last_module;
    
    TeaTable string_methods;
    TeaTable list_methods;
    TeaTable map_methods;
    TeaTable file_methods;
    TeaTable range_methods;

    bool error;

    TeaObjectFiber* fiber;

    TeaObject* objects;
    int gray_count;
    int gray_capacity;
    TeaObject** gray_stack;
} TeaVM;

void tea_init_vm(TeaState* state, TeaVM* vm);
void tea_free_vm(TeaVM* vm);
void tea_runtime_error(TeaVM* vm, const char* format, ...);
TeaInterpretResult tea_interpret_module(TeaState* state, const char* module_name, const char* source);

static inline void tea_push(TeaVM* vm, TeaValue value)
{
    *vm->fiber->stack_top++ = value;
}

static inline TeaValue tea_pop(TeaVM* vm)
{
    return *(--vm->fiber->stack_top);
}

static inline TeaValue tea_peek(TeaVM* vm, int distance)
{
    return vm->fiber->stack_top[-1 - (distance)];
}

#endif