#include <stdlib.h>
#include <time.h>

#include "modules/tea_time.h"
#include "vm/tea_vm.h"
#include "vm/tea_native.h"

static TeaValue clock_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(clock, 0);

    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static TeaValue time_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(time, 0);

    return NUMBER_VAL((double)time(NULL));
}

TeaValue tea_import_time(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, "time", 4);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "clock", clock_native);
    tea_native_function(vm, &module->values, "time", time_native);

    return OBJECT_VAL(module);
}