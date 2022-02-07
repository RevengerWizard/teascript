#ifndef TEA_VM_H
#define TEA_VM_H

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

typedef struct
{
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

    TeaObjectString* init_string;
    TeaObjectUpvalue* open_upvalues;

    size_t bytes_allocated;
    size_t next_GC;
    TeaObject* objects;
    int gray_count;
    int gray_capacity;
    TeaObject** gray_stack;
} TeaVM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} TeaInterpretResult;

extern TeaVM vm;

void tea_init_vm();
void tea_free_vm();
void tea_runtime_error(const char* format, ...);
TeaInterpretResult tea_interpret(const char* module_name, const char* source);
void tea_push(TeaValue value);
TeaValue tea_pop();
bool tea_is_falsey(TeaValue value);

#endif