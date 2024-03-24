/*
** tea_gc.c
** Garbage collector
*/

#define tea_gc_c
#define TEA_CORE

#include <stdlib.h>

#include "tea_state.h"
#include "tea_gc.h"
#include "tea_parse.h"
#include "tea_tab.h"

#ifdef TEA_DEBUG_LOG_GC
#include <stdio.h>
#include "tea_debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

/* -- Collector -------------------------------------------------- */

static void gc_blacken(tea_State* T, GCobj* object)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("%p blacken %s\n", (void*)object, tea_typename(setgcV(object)));
#endif

    switch(object->gct)
    {
        case TEA_TFILE:
        {
            GCfile* file = (GCfile*)object;
            tea_gc_markobj(T, (GCobj*)file->path);
            tea_gc_markobj(T, (GCobj*)file->type);
            break;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = (GCmodule*)object;
            tea_gc_markobj(T, (GCobj*)module->name);
            tea_gc_markobj(T, (GCobj*)module->path);
            tea_tab_mark(T, &module->values);
            break;
        }
        case TEA_TLIST:
        {
            GClist* list = (GClist*)object;
            for(int i = 0; i < list->count; i++)
            {
                tea_gc_markval(T, list->items + i);
            }
            break;
        }
        case TEA_TMAP:
        {
            GCmap* map = (GCmap*)object;
            for(int i = 0; i < map->size; i++)
            {
                MapEntry* item = &map->entries[i];
                tea_gc_markval(T, &item->key);
                tea_gc_markval(T, &item->value);
            }
            break;
        }
        case TEA_TMETHOD:
        {
            GCmethod* bound = (GCmethod*)object;
            tea_gc_markval(T, &bound->receiver);
            tea_gc_markobj(T, (GCobj*)bound->method);
            break;
        }
        case TEA_TCLASS:
        {
            GCclass* klass = (GCclass*)object;
            tea_gc_markobj(T, (GCobj*)klass->name);
            tea_gc_markobj(T, (GCobj*)klass->super);
            tea_tab_mark(T, &klass->statics);
            tea_tab_mark(T, &klass->methods);
            break;
        }
        case TEA_TFUNC:
        {
            GCfunc* func = (GCfunc*)object;
            if(!isteafunc(func))
                break;
            tea_gc_markobj(T, (GCobj*)func->t.proto);
            if(func->t.upvalues != NULL)
            {
                for(int i = 0; i < func->t.upvalue_count; i++)
                {
                    tea_gc_markobj(T, (GCobj*)func->t.upvalues[i]);
                }
            }
            break;
        }
        case TEA_TPROTO:
        {
            GCproto* proto = (GCproto*)object;
            tea_gc_markobj(T, (GCobj*)proto->name);
            for(int i = 0; i < proto->k_count; i++)
            {
                tea_gc_markval(T, proto->k + i);
            }
            break;
        }
        case TEA_TINSTANCE:
        {
            GCinstance* instance = (GCinstance*)object;
            tea_gc_markobj(T, (GCobj*)instance->klass);
            tea_tab_mark(T, &instance->fields);
            break;
        }
        case TEA_TUPVALUE:
        {
            GCupvalue* uv = (GCupvalue*)object;
            tea_gc_markval(T, &uv->closed);
            break;
        }
        case TEA_TSTRING:
        case TEA_TRANGE:
            break;
    }
}

static void gc_free(tea_State* T, GCobj* object)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("%p free %s\n", (void*)object, tea_typename(setgcV(object)));
#endif

    switch(object->gct)
    {
        case TEA_TRANGE:
        {
            tea_mem_freet(T, GCrange, object);
            break;
        }
        case TEA_TFILE:
        {
            GCfile* file = (GCfile*)object;
            if((file->is_open == true) && file->file != NULL)
            {
                fclose(file->file);
            }
            tea_mem_freet(T, GCfile, object);
            break;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = (GCmodule*)object;
            tea_tab_free(T, &module->values);
            tea_mem_freet(T, GCmodule, object);
            break;
        }
        case TEA_TLIST:
        {
            GClist* list = (GClist*)object;
            tea_mem_freevec(T, TValue, list->items, list->size);
		    list->items = NULL;
            list->count = 0;
            list->size = 0;
            tea_mem_freet(T, GClist, object);
            break;
        }
        case TEA_TMAP:
        {
            GCmap* map = (GCmap*)object;
            tea_mem_freevec(T, MapEntry, map->entries, map->size);
            tea_mem_freet(T, GCmap, object);
            break;
        }
        case TEA_TMETHOD:
        {
            tea_mem_freet(T, GCmethod, object);
            break;
        }
        case TEA_TCLASS:
        {
            GCclass* klass = (GCclass*)object;
            tea_tab_free(T, &klass->methods);
            tea_tab_free(T, &klass->statics);
            tea_mem_freet(T, GCclass, object);
            break;
        }
        case TEA_TFUNC:
        {
            GCfunc* func = (GCfunc*)object;
            if(isteafunc(func))
            {
                tea_mem_freevec(T, GCupvalue*, func->t.upvalues, func->t.upvalue_count);
                tea_mem_freet(T, GCfuncT, object);
            }
            else
            {
                tea_mem_free(T, object, sizeCfunc(func->c.upvalue_count));
            }
            break;
        }
        case TEA_TPROTO:
        {
            GCproto* proto = (GCproto*)object;
            tea_mem_freevec(T, uint8_t, proto->bc, proto->bc_size);
            tea_mem_freevec(T, LineStart, proto->lines, proto->line_size);
            tea_mem_freevec(T, TValue, proto->k, proto->k_size);
            tea_mem_freet(T, GCproto, object);
            break;
        }
        case TEA_TINSTANCE:
        {
            GCinstance* instance = (GCinstance*)object;
            tea_tab_free(T, &instance->fields);
            tea_mem_freet(T, GCinstance, object);
            break;
        }
        case TEA_TSTRING:
        {
            GCstr* string = (GCstr*)object;
            tea_mem_freevec(T, char, string->chars, string->len + 1);
            tea_mem_freet(T, GCstr, object);
            break;
        }
        case TEA_TUPVALUE:
        {
            tea_mem_freet(T, GCupvalue, object);
            break;
        }
    }
}

/* Mark GC roots */
static void gc_mark_roots(tea_State* T)
{
    for(TValue* slot = T->stack; slot < T->top; slot++)
    {
        tea_gc_markval(T, slot);
    }

    for(CallInfo* ci = T->ci_base; ci <= T->ci; ci++)
    {
        tea_gc_markobj(T, (GCobj*)ci->func);
    }

    for(GCupvalue* upvalue = T->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        tea_gc_markobj(T, (GCobj*)upvalue);
    }

    tea_tab_mark(T, &T->modules);
    tea_tab_mark(T, &T->globals);

    tea_gc_markobj(T, (GCobj*)T->number_class);
    tea_gc_markobj(T, (GCobj*)T->bool_class);
    tea_gc_markobj(T, (GCobj*)T->list_class);
    tea_gc_markobj(T, (GCobj*)T->map_class);
    tea_gc_markobj(T, (GCobj*)T->string_class);
    tea_gc_markobj(T, (GCobj*)T->range_class);
    tea_gc_markobj(T, (GCobj*)T->file_class);

    tea_gc_markobj(T, (GCobj*)T->constructor_string);
    tea_gc_markobj(T, (GCobj*)T->repl_string);
    tea_gc_markobj(T, (GCobj*)T->memerr);

    for(int i = 0; i < MM__MAX; i++)
    {
        tea_gc_markobj(T, (GCobj*)T->opm_name[i]);
    }

    if(T->parser != NULL)
    {
        tea_parse_mark(T, T->parser);
    }
}

static void gc_trace_references(tea_State* T)
{
    while(T->gc.gray_count > 0)
    {
        GCobj* object = T->gc.gray_stack[--T->gc.gray_count];
        gc_blacken(T, object);
    }
}

static void gc_sweep(tea_State* T)
{
    GCobj* previous = NULL;
    GCobj* object = T->gc.objects;

    while(object != NULL)
    {
        if(object->marked)
        {
            object->marked = false;
            previous = object;
            object = object->next;
        }
        else
        {
            GCobj* unreached = object;
            object = object->next;
            if(previous != NULL)
            {
                previous->next = object;
            }
            else
            {
                T->gc.objects = object;
            }

            gc_free(T, unreached);
        }
    }
}

/* Mark a TValue (if needed) */
void tea_gc_markval(tea_State* T, TValue* value)
{
    if(tvisgcv(value))
        tea_gc_markobj(T, gcV(value));
}

/* Mark a GC object (if needed) */
void tea_gc_markobj(tea_State* T, GCobj* object)
{
    if(object == NULL)
        return;
    if(object->marked)
        return;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p mark %s\n", (void*)object, tea_typename(setgcV(object)));
#endif

    object->marked = true;

    if(T->gc.gray_size < T->gc.gray_count + 1)
    {
        T->gc.gray_size = TEA_MEM_GROW(T->gc.gray_size);
        T->gc.gray_stack = (GCobj**)T->allocf(T->allocd, T->gc.gray_stack, 0, sizeof(GCobj*) * T->gc.gray_size);

        if(T->gc.gray_stack == NULL)
        {
            puts(T->memerr->chars);
            exit(1);
        }
    }

    T->gc.gray_stack[T->gc.gray_count++] = object;
}

/* Perform a GC collection */
void tea_gc_collect(tea_State* T)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = T->gc.bytes_allocated;
#endif

    gc_mark_roots(T);
    gc_trace_references(T);
    tea_tab_white(&T->strings);
    gc_sweep(T);

    T->gc.next_gc = T->gc.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %llu bytes (from %llu to %llu) next at %llu\n", before - T->gc.bytes_allocated, before, T->gc.bytes_allocated, T->next_gc);
#endif
}

/* Free all remaining GC objects */
void tea_gc_freeall(tea_State* T)
{
    GCobj* object = T->gc.objects;
    while(object != NULL)
    {
        GCobj* next = object->next;
        gc_free(T, object);
        object = next;
    }

    /* Free the gray stack */
    T->allocf(T->allocd, T->gc.gray_stack, sizeof(GCobj*) * T->gc.gray_size, 0);
}

/* -- Allocator -------------------------------------------------- */

/* Call pluggable memory allocator to allocate or resize a fragment */
void* tea_mem_realloc(tea_State* T, void* pointer, size_t old_size, size_t new_size)
{
    T->gc.bytes_allocated += new_size - old_size;

#ifdef TEA_DEBUG_TRACE_MEMORY
    printf("total bytes allocated: %zu\nnew allocation: %zu\nold allocation: %zu\n\n", T->gc.bytes_allocated, new_size, old_size);
#endif

    if(new_size > old_size)
    {
#ifdef TEA_DEBUG_STRESS_GC
        tea_gc_collect(T);
#endif

        if(T->gc.bytes_allocated > T->gc.next_gc)
        {
            tea_gc_collect(T);
        }
    }

    void* block = T->allocf(T->allocd, pointer, old_size, new_size);
    if(block == NULL && new_size > 0)
    {
        puts(T->memerr->chars);
        exit(1);
    }
    return block;
}

/* Resize growable vector */
void* tea_mem_grow(tea_State* T, void* pointer, int* size, size_t size_elem, int limit)
{
    size_t new_size = (*size) << 1;
    if(new_size < 8)
        new_size = 8;
    if(new_size > limit)
        new_size = limit;
    void* block = tea_mem_realloc(T, pointer, (*size) * size_elem, new_size * size_elem);
    *size = new_size;
    return block;
}