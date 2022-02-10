#include "modules/tea_math.h"
#include "vm/tea_vm.h"
#include "vm/tea_native.h"

static TeaValue min_native(int arg_count, TeaValue* args)
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
            tea_runtime_error("A non-number value passed to min()");
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

static TeaValue max_native(int arg_count, TeaValue* args)
{
    
}

static TeaValue mid_native(int arg_count, TeaValue* args)
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
        tea_runtime_error("mid() takes three numbers");
        return EMPTY_VAL;
    }
}

static TeaValue floor_native(int arg_count, TeaValue* args)
{
    return EMPTY_VAL;
}

static TeaValue ceil_native(int arg_count, TeaValue* args)
{
    return EMPTY_VAL;
}

static TeaValue round_native(int arg_count, TeaValue* args)
{
    return EMPTY_VAL;
}

static TeaValue cos_native(int arg_count, TeaValue* args)
{
    return EMPTY_VAL;
}

static TeaValue sin_native(int arg_count, TeaValue* args)
{
    return EMPTY_VAL;
}

static TeaValue tan_native(int arg_count, TeaValue* args)
{
    return EMPTY_VAL;
}

static TeaValue sign_native(int arg_count, TeaValue* args)
{
    if(arg_count == 0)
    {
        return NUMBER_VAL(0);
    }

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error("sign() takes a number");
        return EMPTY_VAL;
    }

    double n = AS_NUMBER(args[0]);

    return (n > 0) ? NUMBER_VAL(1) : ((n < 0) ? NUMBER_VAL(-1) : NUMBER_VAL(0));
}

TeaValue tea_import_math(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string("math", 4);
    TeaObjectModule* module = tea_new_module(name);

    tea_native_function(vm, &module->values, "min", min_native);
    tea_native_function(vm, &module->values, "max", max_native);
    tea_native_function(vm, &module->values, "mid", mid_native);
    tea_native_function(vm, &module->values, "floor", floor_native);
    tea_native_function(vm, &module->values, "ceil", ceil_native);
    tea_native_function(vm, &module->values, "round", round_native);
    tea_native_function(vm, &module->values, "cos", cos_native);
    tea_native_function(vm, &module->values, "sin", sin_native);
    tea_native_function(vm, &module->values, "tan", tan_native);
    tea_native_function(vm, &module->values, "sign", sign_native);

    tea_native_property(vm, &module->values, "pi", NUMBER_VAL(M_PI));
    tea_native_property(vm, &module->values, "e", NUMBER_VAL(M_E));

    return OBJECT_VAL(module);
}