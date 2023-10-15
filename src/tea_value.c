/*
** tea_value.c
** Teascript value model and functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#define tea_value_c
#define TEA_CORE

#include "tea_object.h"
#include "tea_string.h"
#include "tea_memory.h"
#include "tea_value.h"

DEFINE_ARRAY(TeaValueArray, TeaValue, value_array)
DEFINE_ARRAY(TeaBytes, uint8_t, bytes)

const char* const tea_val_typenames[] = {
    "null", "number", "bool", 
    "string", "range", "function", "module", "class", "instance", "list", "map", "file", "userdata"
};

const char* tea_val_type(TeaValue a)
{
#ifdef TEA_NAN_TAGGING
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
        return tea_obj_type(a);
    }
#else
    switch(a.type)
    {
        case VAL_BOOL:
            return "bool";
        case VAL_NULL:
            return "null";
        case VAL_NUMBER:
            return "number";
        case VAL_OBJECT:
            return tea_obj_type(a);
        default:
            break;
    }
#endif
    return "unknown";
}

bool tea_val_equal(TeaValue a, TeaValue b)
{
#ifdef TEA_NAN_TAGGING
    if(IS_NUMBER(a) && IS_NUMBER(b))
    {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    else if(IS_OBJECT(a) && IS_OBJECT(b))
    {
        return tea_obj_equal(a, b);
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
            return tea_obj_equal(a, b);
        default:
            return false; /* Unreachable */
    }
#endif
}

bool tea_val_rawequal(TeaValue a, TeaValue b)
{
#ifdef TEA_NAN_TAGGING
    if(IS_NUMBER(a) && IS_NUMBER(b))
    {
        return AS_NUMBER(a) == AS_NUMBER(b);
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
            return false; /* Unreachable */
    }
#endif
}

double tea_val_tonumber(TeaValue value, int* x)
{
    if(x != NULL)
        *x = true;
    if(IS_NUMBER(value))
    {
        return AS_NUMBER(value);
    }
    else if(IS_BOOL(value))
    {
        return AS_BOOL(value) ? 1 : 0;
    }
    else if(IS_STRING(value))
    {
        char* n = AS_CSTRING(value);
        char* end;
        errno = 0;

        double number = strtod(n, &end);

        if(errno != 0 || *end != '\0')
        {
            if(x != NULL)
                *x = false;
            return 0;
        }
        return number;
    }
    else
    {
        if(x != NULL)
            *x = false;
        return 0;
    }
}

TeaOString* tea_val_tostring(TeaState* T, TeaValue value)
{
#ifdef TEA_NAN_TAGGING
    if(IS_BOOL(value))
    {
        return AS_BOOL(value) ? tea_str_literal(T, "true") : tea_str_literal(T, "false");
    }
    else if(IS_NULL(value))
    {
        return tea_str_literal(T, "null");
    }
    else if(IS_NUMBER(value))
    {
        return tea_val_number_tostring(T, AS_NUMBER(value));
    }
    else if(IS_OBJECT(value))
    {
        return tea_obj_tostring(T, value);
    }
#else
    switch(value.type)
    {
        case VAL_BOOL:
            return AS_BOOL(value) ? tea_str_literal(T, "true") : tea_str_literal(T, "false");
        case VAL_NULL:
            return tea_str_literal(T, "null");
        case VAL_NUMBER:
            return tea_val_number_tostring(T, AS_NUMBER(value));
        case VAL_OBJECT:
            return tea_obj_tostring(T, value);
        default:
            break;
    }
#endif
    return tea_str_literal(T, "unknown");
}

TeaOString* tea_val_number_tostring(TeaState* T, double number)
{
    if(isnan(number)) return tea_str_literal(T, "nan");
    if(isinf(number))
    {
        if(number > 0.0)
        {
            return tea_str_literal(T, "infinity");
        }
        else
        {
            return tea_str_literal(T, "-infinity");
        }
    }

    int length = snprintf(NULL, 0, TEA_NUMBER_FMT, number);
    char* string = TEA_ALLOCATE(T, char, length + 1);
    snprintf(string, length + 1, TEA_NUMBER_FMT, number);

    return tea_str_take(T, string, length);
}