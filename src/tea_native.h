#ifndef TEA_NATIVE_H
#define TEA_NATIVE_H

#include <stdarg.h>

#include "tea_value.h"
#include "tea_object.h"
#include "tea_vm.h"

#define NATIVE_ERROR(...) \
    do \
    { \
        *error = true; \
        tea_runtime_error(vm, __VA_ARGS__); \
        return EMPTY_VAL; \
    } \
    while(false)

#define VALIDATE_ARG_COUNT(func_name, num_arg) \
    do \
    { \
        if(arg_count != num_arg) \
        { \
            NATIVE_ERROR(#func_name "() expected " #num_arg " arguments but got %d.", arg_count); \
        } \
    } \
    while(false)

#define RANGE_ARG_COUNT(func_name, num_arg, s) \
    do \
    { \
        if(arg_count > num_arg) \
        { \
            NATIVE_ERROR(#func_name "() expected either %s arguments (got %d).", s, arg_count); \
        } \
    } \
    while(false)

#define NATIVE_FUNCTION(name) static TeaValue name##_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)

void tea_native_property(TeaVM* vm, TeaTable* table, const char* name, TeaValue value);
void tea_native_function(TeaVM* vm, TeaTable* table, const char* name, TeaNativeFunction function);

void tea_define_natives(TeaVM* vm);

#endif