#include <stdlib.h>
#include <time.h>

#include "tea_module.h"
#include "tea_vm.h"
#include "tea_native.h"

static TeaValue clock_time(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(clock, 0);

    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static TeaValue time_time(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(time, 0);

    return NUMBER_VAL((double)time(NULL));
}

TeaValue tea_import_time(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_TIME_MODULE, 4);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "clock", clock_time);
    tea_native_function(vm, &module->values, "time", time_time);

    return OBJECT_VAL(module);
}