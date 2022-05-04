#ifndef TEA_CORE_H
#define TEA_CORE_H

#include <stdarg.h>

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


void tea_native_property(TeaVM* vm, TeaTable* table, const char* name, TeaValue value);
void tea_native_function(TeaVM* vm, TeaTable* table, const char* name, TeaNativeFunction function);

void tea_open_core(TeaVM* vm);

#endif