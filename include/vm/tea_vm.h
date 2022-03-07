#ifndef TEA_VM_H
#define TEA_VM_H

#include "tea_predefines.h"
#include "state/tea_state.h"
#include "vm/tea_object.h"
#include "util/tea_table.h"
#include "vm/tea_value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    TeaObjectClosure* closure;
    uint8_t* ip;
    TeaValue* slots;
} TeaCallFrame;

typedef struct TeaVM
{
    TeaState* state;

    TeaCallFrame frames[FRAMES_MAX];
    int frame_count;

    TeaValue stack[STACK_MAX];
    TeaValue* stack_top;
    
    TeaObjectModule* last_module;
    TeaTable modules;
    TeaTable globals;
    TeaTable strings;

    TeaTable string_methods;
    TeaTable list_methods;
    TeaTable file_methods;

    TeaObjectUpvalue* open_upvalues;

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
    *vm->stack_top++ = value;
}

static inline TeaValue tea_pop(TeaVM* vm)
{
    return *(--vm->stack_top);
}

static inline TeaValue tea_peek(TeaVM* vm, int distance)
{
    return vm->stack_top[-1 - distance];
}

#endif