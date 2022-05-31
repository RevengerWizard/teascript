#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_module.h"
#include "tea_core.h"

#ifdef _WIN32
#define unsetenv(NAME) _putenv_s(NAME, "")
int setenv(const char* name, const char* value, int overwrite) 
{
    if(!overwrite && getenv(name) == NULL)
    {
        return 0;
    }

    if(_putenv_s(name, value))
    {
        return -1;
    } 
    else
    {
        return 0;
    }
}
#endif

static TeaValue getenv_os(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1 && count != 2)
    {
        tea_runtime_error(vm, "getenv() expected either 1 or 2 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "getenv() argument takes a string");
        return EMPTY_VAL;
    }

    char* value = getenv(AS_CSTRING(args[0]));

    if(count == 2)
    {
        if(!IS_STRING(args[1]))
        {
            tea_runtime_error(vm, "getenv() arguments must be a string.");
            return EMPTY_VAL;
        }

        if(value != NULL) 
        {
            return OBJECT_VAL(tea_copy_string(vm->state, value, strlen(value)));
        }

        return args[1];
    }

    if(value != NULL) 
    {
        return OBJECT_VAL(tea_copy_string(vm->state, value, strlen(value)));
    }

    return NULL_VAL;
}

static TeaValue setenv_os(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "setenv() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]) || (!IS_STRING(args[1]) && !IS_NULL(args[1])))
    {
        tea_runtime_error(vm, "setenv() arguments must be a string or a null.");
        return EMPTY_VAL;
    }

    char* key = AS_CSTRING(args[0]);

    int retval;
    if(IS_NULL(args[1]))
    {
        retval = unsetenv(key);
    }
    else
    {
        retval = setenv(key, AS_CSTRING(args[1]), 1);
    }

    if(retval == -1)
    {
        tea_runtime_error(vm, "Failed to set environment variable");
        return EMPTY_VAL;
    }

    return NULL_VAL;
}

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

static const char* os_name()
{
    #if defined(_WIN32) || defined(_WIN64)
    return "windows";
    #elif defined(__unix) || defined(__unix__)
    return "unix";
    #elif defined(__APPLE__) || defined(__MACH__)
    return "macOS";
    #elif defined(__linux__)
    return "linux";
    #elif defined(__FreeBSD__)
    return "freeBSD";
    #else
    return "other";
    #endif
}

TeaValue tea_import_os(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_OS_MODULE, 2);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    const char* os = os_name();
    tea_native_value(vm, &module->values, "name", OBJECT_VAL(tea_copy_string(vm->state, os, strlen(os))));

    tea_native_function(vm, &module->values, "getenv", getenv_os);
    tea_native_function(vm, &module->values, "setenv", setenv_os);
    tea_native_function(vm, &module->values, "system", system_os);

    return OBJECT_VAL(module);
}