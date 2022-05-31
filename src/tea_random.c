#include <stdlib.h>
#include <time.h>

#include "tea_module.h"
#include "tea_core.h"

static TeaValue random_random(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "random() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    return NUMBER_VAL(((double)rand() * (1 - 0)) / (double)RAND_MAX + 0);
}

static TeaValue range_random(TeaVM* vm, int count, TeaValue* args)
{
    if(count < 1 || count > 2)
    {
        tea_runtime_error(vm, "range() expected either 1 or 2 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    if(IS_NUMBER(args[0]) && IS_NUMBER(args[1]))
    {
        int upper = AS_NUMBER(args[1]);
        int lower = AS_NUMBER(args[0]);

        return NUMBER_VAL((rand() % (upper - lower + 1)) + lower);
    }
    else if(IS_RANGE(args[0]) && count == 1)
    {
        TeaObjectRange* range = AS_RANGE(args[0]);
        int upper = range->to;
        int lower = range->from;
        if(!range->inclusive) upper -= 1;

        return NUMBER_VAL((rand() % (upper - lower + 1)) + lower);
    }
    else
    {
        tea_runtime_error(vm, "range() takes two numbers or a range");
        return EMPTY_VAL;
    }
}

static TeaValue choice_random(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "choice() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_LIST(args[0]))
    {
        tea_runtime_error(vm, "choice() argument must be a list");
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(args[0]);
    count = list->items.count;
    args = list->items.values;

    int index = rand() % count;

    return args[index];
}

TeaValue tea_import_random(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_RANDOM_MODULE, 6);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "random", random_random);
    tea_native_function(vm, &module->values, "range", range_random);
    tea_native_function(vm, &module->values, "choice", choice_random);

    return OBJECT_VAL(module);
}