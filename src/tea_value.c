#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tea_object.h"
#include "tea_memory.h"
#include "tea_value.h"

DEFINE_ARRAY(TeaValueArray, TeaValue, value_array)
DEFINE_ARRAY(TeaBytes, uint8_t, bytes)

const char* tea_value_type(TeaValue a)
{
    if(IS_BOOL(a))
    {
        return "bool";
    }
    else if(IS_NULL(a))
    {
        return "null";
    }
    else if(IS_NUMBER(a))
    {
        return "number";
    }
    else if(IS_OBJECT(a))
    {
        return tea_object_type(a);
    }

    return "unknown";
}

bool tea_values_equal(TeaValue a, TeaValue b)
{
#ifdef NAN_TAGGING
    if(IS_NUMBER(a) && IS_NUMBER(b))
    {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    else if(IS_OBJECT(a) && IS_OBJECT(b))
    {
        return tea_objects_equal(a, b);
    }
    return a == b;
#else
    if(a.type != b.type)
        return false;
    switch(a.type)
    {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJECT:
            return AS_OBJECT(a) == AS_OBJECT(b);
        default:
            return false; // Unreachable.
    }
#endif
}

char* tea_value_tostring(TeaState* state, TeaValue value)
{
#ifdef NAN_TAGGING
    if(IS_BOOL(value))
    {
        return AS_BOOL(value) ? "true" : "false";
    }
    else if(IS_NULL(value))
    {
        return "null";
    }
    else if(IS_NUMBER(value))
    {
        double number = AS_NUMBER(value);
        if(isnan(number)) return "nan";
        if(isinf(number))
        {
            if(number > 0.0)
            {
                return "infinity";
            }
            else
            {
                return "-infinity";
            }
        }

        int length = snprintf(NULL, 0, "%.15g", number) + 1;
        char* string = ALLOCATE(state, char, length);
        snprintf(string, length, "%.15g", number);

        return string;
    }
    else if(IS_OBJECT(value))
    {
        return tea_object_tostring(value);
    }

    return "unknown";
#else
    // Support
#endif
}

void tea_print_value(TeaValue value)
{
#ifdef NAN_TAGGING
    if(IS_BOOL(value))
    {
        printf(AS_BOOL(value) ? "true" : "false");
    }
    else if(IS_NULL(value))
    {
        printf("null");
    }
    else if(IS_NUMBER(value))
    {
        printf("%.15g", AS_NUMBER(value));
    }
    else if(IS_OBJECT(value))
    {
        tea_print_object(value);
    }

    return;
#else
    switch(value.type)
    {
        case VAL_BOOL:
            printf(AS_BOOL(value) ? "true" : "false");
            break;
        case VAL_NULL:
            printf("null");
            break;
        case VAL_NUMBER:
            printf("%.15g", AS_NUMBER(value));
            break;
        case VAL_OBJECT:
            tea_print_object(value);
            break;
    }
#endif
}