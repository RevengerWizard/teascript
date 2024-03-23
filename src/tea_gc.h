/*
** tea_gc.h
** Garbage collector
*/

#ifndef _TEA_GC_H
#define _TEA_GC_H

#include "tea_obj.h"

/* Collector */
TEA_FUNC void tea_gc_markobj(tea_State* T, GCobj* object);
TEA_FUNC void tea_gc_markval(tea_State* T, TValue* value);
TEA_FUNC void tea_gc_collect(tea_State* T);
TEA_FUNC void tea_gc_freeall(tea_State* T);

/* Allocator */
TEA_FUNC void* tea_mem_grow(tea_State* T, void* pointer, int* size, size_t size_elem, int limit);
TEA_FUNC void* tea_mem_realloc(tea_State* T, void* pointer, size_t old_size, size_t new_size);

#define tea_mem_new(T, type, count) \
    (type*)tea_mem_realloc(T, NULL, 0, sizeof(type) * (count))

static TEA_AINLINE void tea_mem_free(tea_State* T, void* pointer, size_t old_size)
{
    T->gc.bytes_allocated -= old_size;
    T->allocf(T->allocd, pointer, old_size, 0);
}

#define tea_mem_freet(T, type, pointer) tea_mem_free(T, pointer, sizeof(type))

#define TEA_MEM_GROW(size) \
    ((size) < 8 ? 8 : (size) * 2)

#define tea_mem_growvec(T, type, pointer, size, limit) \
    (type*)tea_mem_grow(T, pointer, &(size), sizeof(type), limit)

#define tea_mem_reallocvec(T, type, pointer, old_count, new_count) \
    (type*)tea_mem_realloc(T, pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define tea_mem_freevec(T, type, pointer, old_count) \
    tea_mem_free(T, pointer, sizeof(type) * (old_count))

#endif