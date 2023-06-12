/*
** tea_state.h
** Teascript global state
*/

#ifndef TEA_STATE_H
#define TEA_STATE_H

#include <setjmp.h>

#include "tea.h"

#include "tea_def.h"
#include "tea_value.h"
#include "tea_object.h"

#define BASIC_CI_SIZE 8
#define BASE_STACK_SIZE (TEA_MIN_STACK * 2)

typedef struct
{
    TeaObjectClosure* closure;
    TeaObjectNative* native;
    uint8_t* ip;
    TeaValue* base;
} TeaCallInfo;

typedef struct TeaState
{
    TeaValue* stack_last;
    TeaValue* stack;
    TeaValue* top;
    TeaValue* base;
    int stack_size;
    TeaCallInfo* ci;
    TeaCallInfo* end_ci;
    TeaCallInfo* base_ci;
    int ci_size;
    TeaObjectUpvalue* open_upvalues;
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
    TeaObject* objects;
    size_t bytes_allocated;
    size_t next_gc;
    int gray_count;
    int gray_capacity;
    TeaObject** gray_stack;
    struct tea_longjmp* error_jump;
    TeaCFunction panic;
    int argc;
    char** argv;
    int argf;
    bool repl;
    int nccalls;
} TeaState;

#define TEA_THROW(T) (longjmp(T->error_jump->buf, 1))
#define TEA_TRY(T) (setjmp(T->error_jump->buf))

TeaObjectClass* tea_state_get_class(TeaState* T, TeaValue value);

#endif