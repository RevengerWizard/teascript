#include "vm/tea_object.h"
#include "memory/tea_memory.h"
#include "vm/tea_value.h"
#include "util/tea_array.h"

void tea_init_value_array(TeaValueArray* array)
{
    array->values = NULL;
    array->capacity = 0;
    array->count = 0;
}

void tea_write_value_array(TeaValueArray* array, TeaValue value)
{
    if(array->capacity < array->count + 1)
    {
        int old_capacity = array->capacity;
        array->capacity = GROW_CAPACITY(old_capacity);
        array->values = GROW_ARRAY(TeaValue, array->values, old_capacity, array->capacity);
    }

    array->values[array->count] = value;
    array->count++;
}

void tea_free_value_array(TeaValueArray* array)
{
    FREE_ARRAY(TeaValue, array->values, array->capacity);
    tea_init_value_array(array);
}