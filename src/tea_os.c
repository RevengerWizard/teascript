#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_module.h"
#include "tea_core.h"

static TeaValue system_os(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(system, 1);

    if(!IS_STRING(args[0]))
    {
        NATIVE_ERROR("system() argument takes a string.");
    }

    const char* s = AS_CSTRING(args[0]);
    if(system(s) == -1)
    {
        NATIVE_ERROR("Unable to execute the command.");
    }

    return EMPTY_VAL;
}

TeaValue tea_import_os(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_OS_MODULE, 2);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "system", system_os);

    return OBJECT_VAL(module);
}