#include <string.h>
#include <math.h>

#include "tea_module.h"
#include "tea_core.h"

static TeaValue min_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    if(arg_count == 0) 
    {
        return NUMBER_VAL(0);
    } 
    else if(arg_count == 1 && IS_LIST(args[0])) 
    {
        TeaObjectList* list = AS_LIST(args[0]);
        arg_count = list->items.count;
        args = list->items.values;
    }

    double minimum = AS_NUMBER(args[0]);

    for(int i = 1; i < arg_count; i++) 
    {
        TeaValue value = args[i];
        if(!IS_NUMBER(value)) 
        {
            NATIVE_ERROR("A non-number value passed to min()");
        }

        double current = AS_NUMBER(value);

        if(minimum > current) 
        {
            minimum = current;
        }
    }

    return NUMBER_VAL(minimum);
}

static TeaValue max_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    if(arg_count == 0) 
    {
        return NUMBER_VAL(0);
    } 
    else if(arg_count == 1 && IS_LIST(args[0])) 
    {
        TeaObjectList* list = AS_LIST(args[0]);
        arg_count = list->items.count;
        args = list->items.values;
    }

    double maximum = AS_NUMBER(args[0]);

    for(int i = 1; i < arg_count; i++) 
    {
        TeaValue value = args[i];
        if(!IS_NUMBER(value)) 
        {
            NATIVE_ERROR("A non-number value passed to min()");
        }

        double current = AS_NUMBER(value);

        if(maximum < current) 
        {
            maximum = current;
        }
    }

    return NUMBER_VAL(maximum);
}

static TeaValue mid_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    if(arg_count == 0)
    {
        return NUMBER_VAL(0);
    }
    else if(arg_count == 1 && IS_LIST(args[0]))
    {
        TeaObjectList* list = AS_LIST(args[0]);
        arg_count = list->items.count;
        args = list->items.values;
    }

    if(arg_count == 3 && IS_NUMBER(args[0]) && IS_NUMBER(args[1]) && IS_NUMBER(args[2]))
    {
        double x = AS_NUMBER(args[0]);
        double y = AS_NUMBER(args[1]);
        double z = AS_NUMBER(args[2]);

        double mid;

        if(x > y)
        {
            if(y > z)
                mid = y;
            else if(x > z)
                mid = z;
            else
                mid = x;
        }
        else
        {
            if(x > z)
                mid = x;
            else if(y > z)
                mid = z;
            else
                mid = y;
        }

        return NUMBER_VAL(mid);
    }
    else
    {
        NATIVE_ERROR("mid() takes three numbers");
    }
}

static TeaValue average_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    double average = 0;

    if(arg_count == 0)
    {
        return NUMBER_VAL(0);
    }
    else if(arg_count == 1 && IS_LIST(args[0]))
    {
        TeaObjectList* list = AS_LIST(args[0]);
        arg_count = list->items.count;
        args = list->items.values;
    }

    for(int i = 1; i < arg_count; i++) 
    {
        TeaValue value = args[i];
        if(!IS_NUMBER(value)) 
        {
            NATIVE_ERROR("A non-number value passed to min()");
        }

        average = average + AS_NUMBER(value);
    }

    return NUMBER_VAL(average);
}

static TeaValue floor_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(floor, 1);

    if(!IS_NUMBER(args[0])) 
    {
        NATIVE_ERROR("A non-number value passed to floor()");
    }

    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

static TeaValue ceil_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(ceil, 1);

    if(!IS_NUMBER(args[0])) 
    {
        NATIVE_ERROR("A non-number value passed to ceil()");
    }

    return NUMBER_VAL(ceil(AS_NUMBER(args[0])));
}

static TeaValue round_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(round, 1);

    if(!IS_NUMBER(args[0])) 
    {
        NATIVE_ERROR("A non-number value passed to round()");
    }

    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

static TeaValue cos_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(cos, 1);

    if(!IS_NUMBER(args[0])) 
    {
        NATIVE_ERROR("A non-number value passed to cos()");
    }

    return NUMBER_VAL(cos(AS_NUMBER(args[0])));
}

static TeaValue sin_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(sin, 1);

    if(!IS_NUMBER(args[0])) 
    {
        NATIVE_ERROR("A non-number value passed to sin()");
    }

    return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

static TeaValue tan_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(tan, 1);

    if(!IS_NUMBER(args[0])) 
    {
        NATIVE_ERROR("A non-number value passed to tan()");
    }

    return NUMBER_VAL(tan(AS_NUMBER(args[0])));
}

static TeaValue sign_math(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    if(arg_count == 0)
    {
        return NUMBER_VAL(0);
    }

    if(!IS_NUMBER(args[0]))
    {
        NATIVE_ERROR("sign() takes a number");
    }

    double n = AS_NUMBER(args[0]);

    return (n > 0) ? NUMBER_VAL(1) : ((n < 0) ? NUMBER_VAL(-1) : NUMBER_VAL(0));
}

TeaValue tea_import_math(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_MATH_MODULE, 4);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    tea_native_function(vm, &module->values, "min", min_math);
    tea_native_function(vm, &module->values, "max", max_math);
    tea_native_function(vm, &module->values, "mid", mid_math);
    tea_native_function(vm, &module->values, "average", average_math);
    tea_native_function(vm, &module->values, "floor", floor_math);
    tea_native_function(vm, &module->values, "ceil", ceil_math);
    tea_native_function(vm, &module->values, "round", round_math);
    tea_native_function(vm, &module->values, "cos", cos_math);
    tea_native_function(vm, &module->values, "sin", sin_math);
    tea_native_function(vm, &module->values, "tan", tan_math);
    tea_native_function(vm, &module->values, "sign", sign_math);

    tea_native_value(vm, &module->values, "pi", NUMBER_VAL(M_PI));
    tea_native_value(vm, &module->values, "e", NUMBER_VAL(M_E));

    return OBJECT_VAL(module);
}