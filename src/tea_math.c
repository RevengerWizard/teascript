#include <string.h>
#include <math.h>

#include "tea_module.h"
#include "tea_core.h"

static TeaValue min_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count == 0) 
    {
        return NUMBER_VAL(0);
    } 
    else if(count == 1 && IS_LIST(args[0])) 
    {
        TeaObjectList* list = AS_LIST(args[0]);
        count = list->items.count;
        args = list->items.values;
    }
    else if(count == 1 && IS_RANGE(args[0]))
    {
        TeaObjectRange* range = AS_RANGE(args[0]);
        return NUMBER_VAL(fmin(range->from, range->to));
    }

    double minimum = AS_NUMBER(args[0]);

    for(int i = 1; i < count; i++) 
    {
        TeaValue value = args[i];
        if(!IS_NUMBER(value)) 
        {
            tea_runtime_error(vm, "A non-number value passed to min()");
            return EMPTY_VAL;
        }

        double current = AS_NUMBER(value);

        if(minimum > current) 
        {
            minimum = current;
        }
    }

    return NUMBER_VAL(minimum);
}

static TeaValue max_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count == 0) 
    {
        return NUMBER_VAL(0);
    } 
    else if(count == 1 && IS_LIST(args[0])) 
    {
        TeaObjectList* list = AS_LIST(args[0]);
        count = list->items.count;
        args = list->items.values;
    }
    else if(count == 1 && IS_RANGE(args[0]))
    {
        TeaObjectRange* range = AS_RANGE(args[0]);
        return NUMBER_VAL(fmax(range->from, range->to));
    }

    double maximum = AS_NUMBER(args[0]);

    for(int i = 1; i < count; i++) 
    {
        TeaValue value = args[i];
        if(!IS_NUMBER(value)) 
        {
            tea_runtime_error(vm, "A non-number value passed to min()");
            return EMPTY_VAL;
        }

        double current = AS_NUMBER(value);

        if(maximum < current) 
        {
            maximum = current;
        }
    }

    return NUMBER_VAL(maximum);
}

static TeaValue mid_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count == 0)
    {
        return NUMBER_VAL(0);
    }
    else if(count == 1 && IS_LIST(args[0]))
    {
        TeaObjectList* list = AS_LIST(args[0]);
        count = list->items.count;
        args = list->items.values;
    }

    if(count == 3 && IS_NUMBER(args[0]) && IS_NUMBER(args[1]) && IS_NUMBER(args[2]))
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
        tea_runtime_error(vm, "mid() takes three numbers");
        return EMPTY_VAL;
    }
}

static TeaValue average_math(TeaVM* vm, int count, TeaValue* args)
{
    double average = 0;

    if(count == 0)
    {
        return NUMBER_VAL(0);
    }
    else if(count == 1 && IS_LIST(args[0]))
    {
        TeaObjectList* list = AS_LIST(args[0]);
        count = list->items.count;
        args = list->items.values;
    }

    for(int i = 1; i < count; i++) 
    {
        TeaValue value = args[i];
        if(!IS_NUMBER(value)) 
        {
            tea_runtime_error(vm, "A non-number value passed to min()");
            return EMPTY_VAL;
        }

        average = average + AS_NUMBER(value);
    }

    return NUMBER_VAL(average);
}

static TeaValue floor_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "floor() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0])) 
    {
        tea_runtime_error(vm, "A non-number value passed to floor()");
        return EMPTY_VAL;
    }

    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

static TeaValue ceil_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "ceil() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0])) 
    {
        tea_runtime_error(vm, "A non-number value passed to ceil()");
        return EMPTY_VAL;
    }

    return NUMBER_VAL(ceil(AS_NUMBER(args[0])));
}

static TeaValue round_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "round() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0])) 
    {
        tea_runtime_error(vm, "A non-number value passed to round()");
        return EMPTY_VAL;
    }

    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

static TeaValue cos_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "cos() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0])) 
    {
        tea_runtime_error(vm, "A non-number value passed to cos()");
        return EMPTY_VAL;
    }

    return NUMBER_VAL(cos(AS_NUMBER(args[0])));
}

static TeaValue sin_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "sin() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0])) 
    {
        tea_runtime_error(vm, "A non-number value passed to sin()");
        return EMPTY_VAL;
    }

    return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

static TeaValue tan_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "tan() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0])) 
    {
        tea_runtime_error(vm, "A non-number value passed to tan()");
        return EMPTY_VAL;
    }

    return NUMBER_VAL(tan(AS_NUMBER(args[0])));
}

static TeaValue sign_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "sign() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "sign() takes a number");
        return EMPTY_VAL;
    }

    double n = AS_NUMBER(args[0]);

    return (n > 0) ? NUMBER_VAL(1) : ((n < 0) ? NUMBER_VAL(-1) : NUMBER_VAL(0));
}

static TeaValue abs_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "abs() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "A non-number value passed to abs()");
        return EMPTY_VAL;
    }

    double n = AS_NUMBER(args[0]);

    return (n < 0) ? NUMBER_VAL(n * -1) : NUMBER_VAL(n);
}

static TeaValue sqrt_math(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "sqrt() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0])) 
    {
        tea_runtime_error(vm, "A non-number value passed to sqrt()");
        return EMPTY_VAL;
    }

    return NUMBER_VAL(sqrt(AS_NUMBER(args[0])));
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
    tea_native_function(vm, &module->values, "abs", abs_math);
    tea_native_function(vm, &module->values, "sqrt", sqrt_math);

    tea_native_value(vm, &module->values, "pi", NUMBER_VAL(3.14159265358979323846));
    tea_native_value(vm, &module->values, "e", NUMBER_VAL(2.71828182845904523536));
    tea_native_value(vm, &module->values, "phi", NUMBER_VAL(1.61803398874989484820));

    return OBJECT_VAL(module);
}