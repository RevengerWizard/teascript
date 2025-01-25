/*
** tea_gc.h
** Garbage collector
*/

#ifndef _TEA_GC_H
#define _TEA_GC_H

#include "tea_obj.h"

#define TEA_GC_FINALIZED 0x10
#define TEA_GC_FIXED 0x20

#define fix_string(s) (((GCobj*)(s))->marked |= TEA_GC_FIXED)
#define mark_finalized(x) (((GCobj*)(x))->marked |= TEA_GC_FINALIZED)

/* Collector */
TEA_FUNC void tea_gc_separateudata(tea_State* T);
TEA_FUNC void tea_gc_finalize_udata(tea_State* T);
TEA_FUNC void tea_gc_collect(tea_State* T);
TEA_FUNC void tea_gc_freeall(tea_State* T);

/* Allocator */
TEA_FUNC void* tea_mem_grow(tea_State* T, void* p, uint32_t* size, size_t size_elem, int limit);
TEA_FUNC GCobj* tea_mem_newgco(tea_State* T, size_t size, uint8_t type);
TEA_FUNC void* tea_mem_realloc(tea_State* T, void* p, size_t old_size, size_t new_size);

#define tea_mem_new(T, size) tea_mem_realloc(T, NULL, 0, (size));

static TEA_AINLINE void tea_mem_free(tea_State* T, void* p, size_t old_size)
{
    T->gc.total -= old_size;
    T->allocf(T->allocd, p, old_size, 0);
}

#define tea_mem_freet(T, p) tea_mem_free(T, (p), sizeof(*(p)))

#define tea_mem_newobj(T, type, object_type) (type*)tea_mem_newgco(T, sizeof(type), object_type)

#define TEA_MEM_GROW(size) \
    ((size) < 8 ? 8 : (size) * 2)

#define tea_mem_newvec(T, type, count) \
    (type*)tea_mem_realloc(T, NULL, 0, sizeof(type) * (count))

#define tea_mem_growvec(T, type, p, size, limit) \
    (type*)tea_mem_grow(T, p, &(size), sizeof(type), limit)

#define tea_mem_reallocvec(T, type, p, old_count, new_count) \
    (type*)tea_mem_realloc(T, p, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define tea_mem_freevec(T, type, p, old_count) \
    tea_mem_free(T, p, sizeof(type) * (old_count))

#endif