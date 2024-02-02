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

void tea_gc_markobj(tea_State* T, GCobj* object)
{
    if(object == NULL)
        return;
    if(object->marked)
        return;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p mark %s\n", (void*)object, tea_val_type(OBJECT_VAL(object)));
#endif

    object->marked = true;

    if(T->gray_size < T->gray_count + 1)
    {
        T->gray_size = TEA_MEM_GROW(T->gray_size);
        T->gray_stack = (GCobj**)((*T->allocf)(T->allocd, T->gray_stack, 0, sizeof(GCobj*) * T->gray_size));

        if(T->gray_stack == NULL)
        {
            puts(T->memerr->chars);
            exit(1);
        }
    }

    T->gray_stack[T->gray_count++] = object;
}

void tea_gc_markval(tea_State* T, Value value)
{
    if(IS_OBJECT(value))
        tea_gc_markobj(T, AS_OBJECT(value));
}

static void gc_blacken_object(tea_State* T, GCobj* object)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("%p blacken %s\n", (void*)object, tea_val_type(OBJECT_VAL(object)));
#endif

    switch(object->type)
    {
        case OBJ_FILE:
        {
            GCfile* file = (GCfile*)object;
            tea_gc_markobj(T, (GCobj*)file->path);
            tea_gc_markobj(T, (GCobj*)file->type);
            break;
        }
        case OBJ_MODULE:
        {
            GCmodule* module = (GCmodule*)object;
            tea_gc_markobj(T, (GCobj*)module->name);
            tea_gc_markobj(T, (GCobj*)module->path);
            tea_tab_mark(T, &module->values);
            break;
        }
        case OBJ_LIST:
        {
            GClist* list = (GClist*)object;
            for(int i = 0; i < list->count; i++)
            {
                tea_gc_markval(T, list->items[i]);
            }
            break;
        }
        case OBJ_MAP:
        {
            GCmap* map = (GCmap*)object;
            for(int i = 0; i < map->size; i++)
            {
                MapEntry* item = &map->entries[i];
                tea_gc_markval(T, item->key);
                tea_gc_markval(T, item->value);
            }
            break;
        }
        case OBJ_METHOD:
        {
            GCmethod* bound = (GCmethod*)object;
            tea_gc_markval(T, bound->receiver);
            tea_gc_markval(T, bound->method);
            break;
        }
        case OBJ_CLASS:
        {
            GCclass* klass = (GCclass*)object;
            tea_gc_markobj(T, (GCobj*)klass->name);
            tea_gc_markobj(T, (GCobj*)klass->super);
            tea_tab_mark(T, &klass->statics);
            tea_tab_mark(T, &klass->methods);
            break;
        }
        case OBJ_FUNC:
        {
            GCfuncT* func = (GCfuncT*)object;
            tea_gc_markobj(T, (GCobj*)func->proto);
            if(func->upvalues != NULL)
            {
                for(int i = 0; i < func->upvalue_count; i++)
                {
                    tea_gc_markobj(T, (GCobj*)func->upvalues[i]);
                }
            }
            break;
        }
        case OBJ_PROTO:
        {
            GCproto* proto = (GCproto*)object;
            tea_gc_markobj(T, (GCobj*)proto->name);
            for(int i = 0; i < proto->k_count; i++)
            {
                tea_gc_markval(T, proto->k[i]);
            }
            break;
        }
        case OBJ_INSTANCE:
        {
            GCinstance* instance = (GCinstance*)object;
            tea_gc_markobj(T, (GCobj*)instance->klass);
            tea_tab_mark(T, &instance->fields);
            break;
        }
        case OBJ_UPVALUE:
        {
            tea_gc_markval(T, ((GCupvalue*)object)->closed);
            break;
        }
        case OBJ_CFUNC:
        case OBJ_STRING:
        case OBJ_RANGE:
            break;
    }
}

static void gc_free_object(tea_State* T, GCobj* object)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("%p free %s\n", (void*)object, tea_val_type(OBJECT_VAL(object)));
#endif

    switch(object->type)
    {
        case OBJ_RANGE:
        {
            tea_mem_free(T, GCrange, object);
            break;
        }
        case OBJ_FILE:
        {
            GCfile* file = (GCfile*)object;
            if((file->is_open == true) && file->file != NULL)
            {
                fclose(file->file);
            }
            tea_mem_free(T, GCfile, object);
            break;
        }
        case OBJ_MODULE:
        {
            GCmodule* module = (GCmodule*)object;
            tea_tab_free(T, &module->values);
            tea_mem_free(T, GCmodule, object);
            break;
        }
        case OBJ_LIST:
        {
            GClist* list = (GClist*)object;
            tea_mem_freevec(T, Value, list->items, list->size);
		    list->items = NULL;
            list->count = 0;
            list->size = 0;
            tea_mem_free(T, GClist, object);
            break;
        }
        case OBJ_MAP:
        {
            GCmap* map = (GCmap*)object;
            tea_mem_freevec(T, MapEntry, map->entries, map->size);
            tea_mem_free(T, GCmap, object);
            break;
        }
        case OBJ_METHOD:
        {
            tea_mem_free(T, GCmethod, object);
            break;
        }
        case OBJ_CLASS:
        {
            GCclass* klass = (GCclass*)object;
            tea_tab_free(T, &klass->methods);
            tea_tab_free(T, &klass->statics);
            tea_mem_free(T, GCclass, object);
            break;
        }
        case OBJ_FUNC:
        {
            GCfuncT* func = (GCfuncT*)object;
            tea_mem_freevec(T, GCupvalue *, func->upvalues, func->upvalue_count);
            tea_mem_free(T, GCfuncT, object);
            break;
        }
        case OBJ_PROTO:
        {
            GCproto* proto = (GCproto*)object;
            tea_mem_freevec(T, uint8_t, proto->code, proto->size);
            tea_mem_freevec(T, LineStart, proto->lines, proto->line_size);
            tea_mem_freevec(T, Value, proto->k, proto->k_size);
            tea_mem_free(T, GCproto, object);
            break;
        }
        case OBJ_INSTANCE:
        {
            GCinstance* instance = (GCinstance*)object;
            tea_tab_free(T, &instance->fields);
            tea_mem_free(T, GCinstance, object);
            break;
        }
        case OBJ_STRING:
        {
            GCstr* string = (GCstr*)object;
            tea_mem_freevec(T, char, string->chars, string->len + 1);
            tea_mem_free(T, GCstr, object);
            break;
        }
        case OBJ_UPVALUE:
        {
            tea_mem_free(T, GCupvalue, object);
            break;
        }
        case OBJ_CFUNC:
        {
            tea_mem_free(T, GCfuncC, object);
            break;
        }
    }
}

static void gc_mark_roots(tea_State* T)
{
    for(Value* slot = T->stack; slot < T->top; slot++)
    {
        tea_gc_markval(T, *slot);
    }

    for(CallInfo* ci = T->ci_base; ci <= T->ci; ci++)
    {
        tea_gc_markobj(T, (GCobj*)ci->func);
        tea_gc_markobj(T, (GCobj*)ci->cfunc);
    }

    for(GCupvalue* upvalue = T->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        tea_gc_markobj(T, (GCobj*)upvalue);
    }

    tea_tab_mark(T, &T->modules);
    tea_tab_mark(T, &T->globals);

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
    while(T->gray_count > 0)
    {
        GCobj* object = T->gray_stack[--T->gray_count];
        gc_blacken_object(T, object);
    }
}

static void gc_sweep(tea_State* T)
{
    GCobj* previous = NULL;
    GCobj* object = T->objects;

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
                T->objects = object;
            }

            gc_free_object(T, unreached);
        }
    }
}

void tea_gc_collect(tea_State* T)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = T->bytes_allocated;
#endif

    gc_mark_roots(T);
    gc_trace_references(T);
    tea_tab_white(&T->strings);
    gc_sweep(T);

    T->next_gc = T->bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %llu bytes (from %llu to %llu) next at %llu\n", before - T->bytes_allocated, before, T->bytes_allocated, T->next_gc);
#endif
}

void tea_gc_freeall(tea_State* T)
{
    GCobj* object = T->objects;

    while(object != NULL)
    {
        GCobj* next = object->next;
        gc_free_object(T, object);
        object = next;
    }

    /* free the gray stack */
    (*T->allocf)(T->allocd, T->gray_stack, sizeof(GCobj*) * T->gray_size, 0);
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

/* Call pluggable memory allocator to allocate or resize a fragment */
void* tea_mem_realloc(tea_State* T, void* pointer, size_t old_size, size_t new_size)
{
    T->bytes_allocated += new_size - old_size;

#ifdef TEA_DEBUG_TRACE_MEMORY
    printf("total bytes allocated: %zu\nnew allocation: %zu\nold allocation: %zu\n\n", T->bytes_allocated, new_size, old_size);
#endif

    if(new_size > old_size)
    {
#ifdef TEA_DEBUG_STRESS_GC
        tea_gc_collect(T);
#endif

        if(T->bytes_allocated > T->next_gc)
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