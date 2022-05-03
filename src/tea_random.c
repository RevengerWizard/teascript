#include <stdlib.h>
#include <time.h>

#include "tea_module.h"
#include "tea_native.h"

static TeaValue random_random(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(random, 0);

    return NUMBER_VAL(((double)rand() * (1 - 0)) / (double)RAND_MAX + 0);
}

static TeaValue range_random(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    RANGE_ARG_COUNT(range, 2, "1 or 2");

    if(IS_NUMBER(args[0]) && IS_NUMBER(args[1]))
    {
        int upper = AS_NUMBER(args[1]);
        int lower = AS_NUMBER(args[0]);

        return NUMBER_VAL((rand() % (upper - lower + 1)) + lower);
    }
    else if(IS_RANGE(args[0]) && arg_count == 1)
    {
        TeaObjectRange* range = AS_RANGE(args[0]);
        int upper = range->to;
        int lower = range->from;
        if(!range->inclusive) upper -= 1;

        return NUMBER_VAL((rand() % (upper - lower + 1)) + lower);
    }
    else
    {
        NATIVE_ERROR("range() takes two numbers or a range.");
    }
}

static TeaValue choice_random(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(choice, 1);

    if(!IS_LIST(args[0]))
    {
        NATIVE_ERROR("choice() argument must be a list.");
    }

    TeaObjectList* list = AS_LIST(args[0]);
    arg_count = list->items.count;
    args = list->items.values;

    int index = rand() % arg_count;

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