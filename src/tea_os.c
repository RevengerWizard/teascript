#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_module.h"
#include "tea_core.h"

static TeaValue system_os(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "system() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "system() argument takes a string");
        return EMPTY_VAL;
    }

    const char* s = AS_CSTRING(args[0]);
    if(system(s) == -1)
    {
        tea_runtime_error(vm, "Unable to execute the command");
        return EMPTY_VAL;
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