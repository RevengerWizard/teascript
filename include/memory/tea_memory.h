#ifndef TEA_MEMORY_H
#define TEA_MEMORY_H

#include "tea_common.h"
#include "vm/tea_object.h"

#define ALLOCATE(type, count) \
    (type*)tea_reallocate(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) tea_reallocate(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, old_count, new_count) \
    (type*)tea_reallocate(pointer, sizeof(type) * (old_count), \
        sizeof(type) * (new_count))

#define FREE_ARRAY(type, pointer, old_count) \
    tea_reallocate(pointer, sizeof(type) * (old_count), 0)

void* tea_reallocate(void* pointer, size_t old_size, size_t new_size);
void tea_mark_object(TeaObject* object);
void tea_mark_value(TeaValue value);
void tea_collect_garbage();
void tea_free_objects();

#endif