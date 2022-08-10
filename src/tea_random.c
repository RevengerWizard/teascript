#include <stdlib.h>
#include <time.h>
#include <math.h>

#include "tea_module.h"
#include "tea_core.h"

static TeaValue seed_random(TeaVM* vm, int count, TeaValue* args)
{
    if(count > 1)
    {
        tea_runtime_error(vm, "seed() takes 0 or 1 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(count == 0)
    {
        srand(time(NULL));
        return NULL_VAL;
    }

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "seed() argument must be a number");
        return EMPTY_VAL;
    }

    srand(AS_NUMBER(args[0]));

    return NULL_VAL;
}

static TeaValue random_random(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "random() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    return NUMBER_VAL(((double)rand()) / (double)RAND_MAX);
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

static TeaValue shuffle_random(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "shuffle() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_LIST(args[0]))
    {
        tea_runtime_error(vm, "shuffle() argument must be a list");
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(args[0]);

    if(list->items.count <= 1) return args[0];

    for(int i = 0; i < list->items.count - 1; i++)
    {
        int j = floor(i + rand() / (RAND_MAX / (list->items.count - i) + 1));
        TeaValue value = list->items.values[j];
        list->items.values[j] = list->items.values[i];
        list->items.values[i] = value;
    }

    return OBJECT_VAL(list);
}

TeaValue tea_import_random(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_RANDOM_MODULE, 6);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "seed", seed_random);
    tea_native_function(vm, &module->values, "random", random_random);
    tea_native_function(vm, &module->values, "range", range_random);
    tea_native_function(vm, &module->values, "choice", choice_random);
    tea_native_function(vm, &module->values, "shuffle", shuffle_random);

    return OBJECT_VAL(module);
}