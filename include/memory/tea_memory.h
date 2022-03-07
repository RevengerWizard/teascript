#ifndef TEA_MEMORY_H
#define TEA_MEMORY_H

#include "tea_predefines.h"
#include "vm/tea_value.h"
#include "tea_common.h"

#define ALLOCATE(state, type, count) \
    (type*)tea_reallocate(state, NULL, 0, sizeof(type) * (count))

#define FREE(state, type, pointer) tea_reallocate(state, pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(state, type, pointer, old_count, new_count) \
    (type*)tea_reallocate(state, pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define FREE_ARRAY(state, type, pointer, old_count) \
    tea_reallocate(state, pointer, sizeof(type) * (old_count), 0)

void* tea_reallocate(TeaState* state, void* pointer, size_t old_size, size_t new_size);

void tea_mark_object(TeaVM* vm, TeaObject* object);
void tea_mark_value(TeaVM* vm, TeaValue value);

void tea_collect_garbage(TeaVM* vm);
void tea_free_objects(TeaState* state, TeaObject* objects);

#endif