// tea_array.h
// Teascript generic array macro

#ifndef TEA_ARRAY_H
#define TEA_ARRAY_H

#include "tea_common.h"
#include "tea_value.h"

#define DECLARE_ARRAY(name, type, shr) \
	typedef struct name \
    { \
		int capacity; \
		int count; \
		type* values; \
	} name; \
    \
	void tea_init_##shr(name* array); \
	void tea_write_##shr(TeaState* T, name* array, type value); \
	void tea_fill_##shr(TeaState* T, name* array, type value, int count); \
	void tea_free_##shr(TeaState* T, name* array);

#define DEFINE_ARRAY(name, type, shr) \
	void tea_init_##shr(name* array) \
    { \
		array->values = NULL; \
		array->capacity = 0; \
		array->count = 0; \
	} \
	void tea_fill_##shr(TeaState* T, name* array, type value, int count) \
    { \
		if(array->capacity < array->count + count) \
        { \
			int old_capacity = array->capacity; \
			array->capacity = GROW_CAPACITY(old_capacity); \
			array->values = GROW_ARRAY(T, type, array->values, old_capacity, array->capacity); \
		} \
		\
		for(int i = 0; i < count; i++) \
		{ \
			array->values[array->count++] = value; \
		} \
	} \
	void tea_write_##shr(TeaState* T, name* array, type value) \
	{ \
		tea_fill_##shr(T, array, value, 1); \
	} \
	void tea_free_##shr(TeaState* T, name* array) \
    { \
		FREE_ARRAY(T, type, array->values, array->capacity); \
		tea_init_##shr(array); \
	}

#endif