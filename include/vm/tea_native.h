#ifndef TEA_NATIVE_H
#define TEA_NATIVE_H

#include "vm/tea_value.h"
#include "vm/tea_object.h"
#include "vm/tea_vm.h"

#define VALIDATE_ARG_COUNT(func_name, num_arg) \
    do \
    { \
        if(arg_count != num_arg) \
        { \
            tea_runtime_error(#func_name "() expected " #num_arg " arguments but got %d.", arg_count); \
            return EMPTY_VAL; \
        } \
    } \
    while(false)

#define RANGE_ARG_COUNT(func_name, num_arg, s) \
    do \
    { \
        if(arg_count > num_arg) \
        { \
            tea_runtime_error(#func_name "() expected either %s arguments (got %d).", s, arg_count); \
            return EMPTY_VAL; \
        } \
    } \
    while(false)

void tea_native_property(TeaVM* vm, TeaTable* table, const char* name, TeaValue value);
void tea_native_function(TeaVM* vm, TeaTable* table, const char* name, TeaNativeFunction function);

void tea_define_natives(TeaVM* vm);

#endif