// tea_memory.h
// Teascript gc and memory functions

#ifndef TEA_MEMORY_H
#define TEA_MEMORY_H

#include "tea_value.h"
#include "tea_common.h"

#define TEA_ALLOCATE(T, type, count) \
    (type*)tea_reallocate(T, NULL, 0, sizeof(type) * (count))

#define TEA_FREE(T, type, pointer) tea_reallocate(T, pointer, sizeof(type), 0)

#define TEA_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define TEA_GROW_ARRAY(T, type, pointer, old_count, new_count) \
    (type*)tea_reallocate(T, pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define TEA_SHRINK_ARRAY(T, type, pointer, old_count, new_count) \
    (type*)tea_reallocate(T, pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define TEA_FREE_ARRAY(T, type, pointer, old_count) \
    tea_reallocate(T, pointer, sizeof(type) * (old_count), 0)

void* tea_reallocate(TeaState* T, void* pointer, size_t old_size, size_t new_size);

void tea_mark_object(TeaState* T, TeaObject* object);
void tea_mark_value(TeaState* T, TeaValue value);

void tea_free_objects(TeaState* T);

int tea_closest_power_of_two(int n);

#endif