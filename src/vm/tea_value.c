#include <stdio.h>
#include <string.h>

#include "vm/tea_object.h"
#include "memory/tea_memory.h"
#include "vm/tea_value.h"

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

char* tea_value_tostring(TeaValue value)
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
        return "";
    }
    else if(IS_OBJECT(value))
    {
        return tea_object_tostring(value);
    }
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
    else if(IS_EMPTY(value))
    {
        return;
    }
    else if(IS_NUMBER(value))
    {
        printf("%.15g", AS_NUMBER(value));
    }
    else if(IS_OBJECT(value))
    {
        tea_print_object(value);
    }
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