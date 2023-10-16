/*
** tea_value.h
** Teascript value model and functions
*/

#ifndef TEA_VALUE_H
#define TEA_VALUE_H

#include <string.h>

#include "tea_def.h"
#include "tea_array.h"

typedef struct TeaState TeaState;
typedef struct TeaCompiler TeaCompiler;
typedef struct TeaObject TeaObject;
typedef struct TeaOString TeaOString;
typedef struct TeaOFile TeaOFile;

#ifdef TEA_NAN_TAGGING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)

#define MASK_TAG        (7)

#define TAG_NULL        (1)
#define TAG_FALSE       (2)
#define TAG_TRUE        (3)
#define TAG_UNUSED1     (4)
#define TAG_UNUSED2     (5)
#define TAG_UNUSED3     (6)
#define TAG_UNUSED4     (7)

typedef uint64_t TeaValue;

#define IS_BOOL(value)      (((value) | 1) == TRUE_VAL)
#define IS_NULL(value)      ((value) == NULL_VAL)
#define IS_NUMBER(value)    (((value) & QNAN) != QNAN)
#define IS_OBJECT(value) \
    (((value) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(value)      ((value) == TRUE_VAL)
#define AS_NUMBER(value) value_to_num(value)
#define AS_OBJECT(value) \
    ((TeaObject*)(uintptr_t)((value) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((TeaValue)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((TeaValue)(uint64_t)(QNAN | TAG_TRUE))
#define NULL_VAL        ((TeaValue)(uint64_t)(QNAN | TAG_NULL))
#define NUMBER_VAL(num) num_to_value(num)
#define OBJECT_VAL(obj) \
    (TeaValue)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

#define GET_TAG(value)  ((int)((value) & MASK_TAG))

static inline double value_to_num(TeaValue value)
{
    double num;
    memcpy(&num, &value, sizeof(TeaValue));

    return num;
}

static inline TeaValue num_to_value(double num)
{
    TeaValue value;
    memcpy(&value, &num, sizeof(double));

    return value;
}

#else

typedef enum
{
    VAL_NULL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJECT
} TeaValueType;

typedef struct
{
    TeaValueType type;
    union
    {
        bool boolean;
        double number;
        TeaObject* obj;
    } as;
} TeaValue;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NULL(value) ((value).type == VAL_NULL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

#define AS_OBJECT(value) ((value).as.obj)
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value) ((TeaValue){VAL_BOOL, {.boolean = value}})
#define FALSE_VAL ((TeaValue){VAL_BOOL, {.boolean = false}})
#define TRUE_VAL ((TeaValue){VAL_BOOL, {.boolean = true}})
#define NULL_VAL ((TeaValue){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(value) ((TeaValue){VAL_NUMBER, {.number = value}})
#define OBJECT_VAL(object) ((TeaValue){VAL_OBJECT, {.obj = (TeaObject*)object}})

#endif

DECLARE_ARRAY(TeaValueArray, TeaValue, value_array)
DECLARE_ARRAY(TeaBytes, uint8_t, bytes)

TEA_FUNC const char* tea_val_type(TeaValue a);
TEA_FUNC bool tea_val_equal(TeaValue a, TeaValue b);
TEA_FUNC bool tea_val_rawequal(TeaValue a, TeaValue b);
TEA_FUNC double tea_val_tonumber(TeaValue value, bool* x);
TEA_FUNC TeaOString* tea_val_tostring(TeaState* T, TeaValue value);
TEA_FUNC TeaOString* tea_val_number_tostring(TeaState* T, double number);

TEA_DATA const char* const tea_val_typenames[];

#endif