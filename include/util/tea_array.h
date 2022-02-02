#ifndef TEA_ARRAY_H
#define TEA_ARRAY_H

#include "vm/tea_value.h"

typedef struct
{
    int capacity;
    int count;
    TeaValue* values;
} TeaValueArray;

void tea_init_value_array(TeaValueArray* array);
void tea_write_value_array(TeaValueArray* array, TeaValue value);
void tea_free_value_array(TeaValueArray* array);

#endif