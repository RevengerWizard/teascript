// tea_state.c
// Teascript global state

#ifndef TEA_STATE_H
#define TEA_STATE_H

#include <setjmp.h>

#include "tea.h"

#include "tea_common.h"
#include "tea_value.h"
#include "tea_object.h"

typedef struct TeaStackInfo
{
    TeaValue* slot;
    int top;
} TeaStackInfo;

typedef struct TeaState
{
    TeaCompiler* compiler;
    TeaTable modules;
    TeaTable globals;
    TeaTable constants;
    TeaTable strings;
    TeaObjectModule* last_module;
    TeaObjectClass* string_class;
    TeaObjectClass* list_class;
    TeaObjectClass* map_class;
    TeaObjectClass* file_class;
    TeaObjectClass* range_class;
    TeaObjectString* constructor_string;
    TeaObjectString* repl_string;
    TeaValue stack[1000];
    TeaValue* slot;
    int top;
    TeaStackInfo infos[64];
    int info_count;
    TeaObjectThread* thread;
    TeaObject* objects;
    size_t bytes_allocated;
    size_t next_gc;
    int gray_count;
    int gray_capacity;
    TeaObject** gray_stack;
    jmp_buf error_jump;
    int argc;
    const char** argv;
    bool repl;
} TeaState;

#define tea_exit_jump(T) (longjmp(T->error_jump, 1))
#define tea_set_jump(T) (setjmp(T->error_jump))

TeaObjectClass* tea_get_class(TeaState* T, TeaValue value);

static inline void tea_push_slot(TeaState* T, TeaValue value)
{
    T->slot[T->top++] = value;
}

static inline void tea_pop_slot(TeaState* T)
{
    T->top--;
}

#endif