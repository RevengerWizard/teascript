/*
** tea_memory.h
** Teascript memory functions
*/

#ifndef TEA_MEMORY_H
#define TEA_MEMORY_H

#include "tea_value.h"
#include "tea_def.h"

#define MEMERR_MESSAGE "not enough memory"

#define TEA_ALLOCATE(T, type, count) \
    (type*)tea_mem_realloc(T, NULL, 0, sizeof(type) * (count))

#define TEA_FREE(T, type, pointer) tea_mem_realloc(T, pointer, sizeof(type), 0)

#define TEA_GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define TEA_GROW_ARRAY(T, type, pointer, old_count, new_count) \
    (type*)tea_mem_realloc(T, pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define TEA_SHRINK_ARRAY(T, type, pointer, old_count, new_count) \
    (type*)tea_mem_realloc(T, pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define TEA_FREE_ARRAY(T, type, pointer, old_count) \
    tea_mem_realloc(T, pointer, sizeof(type) * (old_count), 0)

TEA_FUNC void* tea_mem_realloc(TeaState* T, void* pointer, size_t old_size, size_t new_size);

#endif