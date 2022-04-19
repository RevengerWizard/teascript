#ifndef TEA_ARRAY_H
#define TEA_ARRAY_H

#include "tea_common.h"
#include "tea_predefines.h"

#define DECLARE_ARRAY(name, type, shr) \
	typedef struct name \
    { \
		int capacity; \
		int count; \
		type* values; \
	} name; \
    \
	void tea_init_##shr(name* array); \
	void tea_write_##shr(TeaState* state, name* array, type value); \
	void tea_free_##shr(TeaState* state, name* array);

#define DEFINE_ARRAY(name, type, shr) \
	void tea_init_##shr(name* array) \
    { \
		array->values = NULL; \
		array->capacity = 0; \
		array->count = 0; \
	} \
	\
	void tea_write_##shr(TeaState* state, name* array, type value) \
    { \
		if(array->capacity < array->count + 1) \
        { \
			int old_capacity = array->capacity; \
			array->capacity = GROW_CAPACITY(old_capacity); \
			array->values = GROW_ARRAY(state, type, array->values, old_capacity, array->capacity); \
		} \
		\
		array->values[array->count] = value; \
		array->count++; \
	} \
	void tea_free_##shr(TeaState* state, name* array) \
    { \
		FREE_ARRAY(state, type, array->values, array->capacity); \
		tea_init_##shr(array); \
	}

#endif