/*
** tea_gc.c
** Teascript garbage collector
*/

#include <stdlib.h>

#define tea_gc_c
#define TEA_CORE

#include "tea_state.h"
#include "tea_memory.h"
#include "tea_gc.h"
#include "tea_parser.h"

#ifdef TEA_DEBUG_LOG_GC
#include <stdio.h>
#include "tea_debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void tea_gc_markobj(TeaState* T, TeaObject* object)
{
    if(object == NULL)
        return;
    if(object->is_marked)
        return;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p mark %s\n", (void*)object, tea_val_type(OBJECT_VAL(object)));
#endif

    object->is_marked = true;

    if(T->gray_capacity < T->gray_count + 1)
    {
        T->gray_capacity = TEA_GROW_CAPACITY(T->gray_capacity);
        T->gray_stack = (TeaObject**)((*T->frealloc)(T->ud, T->gray_stack, 0, sizeof(TeaObject*) * T->gray_capacity));

        if(T->gray_stack == NULL)
        {
            puts(T->memerr->chars);
            exit(1);
        }
    }

    T->gray_stack[T->gray_count++] = object;
}

void tea_gc_markval(TeaState* T, TeaValue value)
{
    if(IS_OBJECT(value))
        tea_gc_markobj(T, AS_OBJECT(value));
}

static void mark_array(TeaState* T, TeaValueArray* array)
{
    for(int i = 0; i < array->count; i++)
    {
        tea_gc_markval(T, array->values[i]);
    }
}

static void blacken_object(TeaState* T, TeaObject* object)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("%p blacken %s\n", (void*)object, tea_val_type(OBJECT_VAL(object)));
#endif

    switch(object->type)
    {
        case OBJ_FILE:
        {
            TeaOFile* file = (TeaOFile*)object;
            tea_gc_markobj(T, (TeaObject*)file->path);
            tea_gc_markobj(T, (TeaObject*)file->type);
            break;
        }
        case OBJ_MODULE:
        {
            TeaOModule* module = (TeaOModule*)object;
            tea_gc_markobj(T, (TeaObject*)module->name);
            tea_gc_markobj(T, (TeaObject*)module->path);
            tea_tab_mark(T, &module->values);
            break;
        }
        case OBJ_LIST:
        {
            TeaOList* list = (TeaOList*)object;
            mark_array(T, &list->items);
            break;
        }
        case OBJ_MAP:
        {
            TeaOMap* map = (TeaOMap*)object;
            for(int i = 0; i < map->capacity; i++)
            {
                TeaMapItem* item = &map->items[i];
                tea_gc_markval(T, item->key);
                tea_gc_markval(T, item->value);
            }
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            TeaOBoundMethod* bound = (TeaOBoundMethod*)object;
            tea_gc_markval(T, bound->receiver);
            tea_gc_markval(T, bound->method);
            break;
        }
        case OBJ_CLASS:
        {
            TeaOClass* klass = (TeaOClass*)object;
            tea_gc_markobj(T, (TeaObject*)klass->name);
            tea_gc_markobj(T, (TeaObject*)klass->super);
            tea_tab_mark(T, &klass->statics);
            tea_tab_mark(T, &klass->methods);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaOClosure* closure = (TeaOClosure*)object;
            tea_gc_markobj(T, (TeaObject*)closure->function);
            if(closure->upvalues != NULL)
            {
                for(int i = 0; i < closure->upvalue_count; i++)
                {
                    tea_gc_markobj(T, (TeaObject*)closure->upvalues[i]);
                }
            }
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaOFunction* function = (TeaOFunction*)object;
            tea_gc_markobj(T, (TeaObject*)function->name);
            mark_array(T, &function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaOInstance* instance = (TeaOInstance*)object;
            tea_gc_markobj(T, (TeaObject*)instance->klass);
            tea_tab_mark(T, &instance->fields);
            break;
        }
        case OBJ_UPVALUE:
        {
            tea_gc_markval(T, ((TeaOUpvalue*)object)->closed);
            break;
        }
        case OBJ_USERDATA:
        case OBJ_NATIVE:
        case OBJ_STRING:
        case OBJ_RANGE:
            break;
    }
}

static void free_object(TeaState* T, TeaObject* object)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("%p free %s\n", (void*)object, tea_obj_type(OBJECT_VAL(object)));
#endif

    switch(object->type)
    {
        case OBJ_RANGE:
        {
            TEA_FREE(T, TeaORange, object);
            break;
        }
        case OBJ_FILE:
        {
            TeaOFile* file = (TeaOFile*)object;
            if((file->is_open == true) && file->file != NULL)
            {
                fclose(file->file);
            }
            TEA_FREE(T, TeaOFile, object);
            break;
        }
        case OBJ_MODULE:
        {
            TeaOModule* module = (TeaOModule*)object;
            tea_tab_free(T, &module->values);
            TEA_FREE(T, TeaOModule, object);
            break;
        }
        case OBJ_LIST:
        {
            TeaOList* list = (TeaOList*)object;
            tea_free_value_array(T, &list->items);
            TEA_FREE(T, TeaOList, object);
            break;
        }
        case OBJ_MAP:
        {
            TeaOMap* map = (TeaOMap*)object;
            TEA_FREE_ARRAY(T, TeaMapItem, map->items, map->capacity);
            TEA_FREE(T, TeaOMap, object);
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            TEA_FREE(T, TeaOBoundMethod, object);
            break;
        }
        case OBJ_CLASS:
        {
            TeaOClass* klass = (TeaOClass*)object;
            tea_tab_free(T, &klass->methods);
            tea_tab_free(T, &klass->statics);
            TEA_FREE(T, TeaOClass, object);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaOClosure* closure = (TeaOClosure*)object;
            TEA_FREE_ARRAY(T, TeaOUpvalue *, closure->upvalues, closure->upvalue_count);
            TEA_FREE(T, TeaOClosure, object);
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaOFunction* function = (TeaOFunction*)object;
            tea_chunk_free(T, &function->chunk);
            TEA_FREE(T, TeaOFunction, object);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaOInstance* instance = (TeaOInstance*)object;
            tea_tab_free(T, &instance->fields);
            TEA_FREE(T, TeaOInstance, object);
            break;
        }
        case OBJ_STRING:
        {
            TeaOString* string = (TeaOString*)object;
            TEA_FREE_ARRAY(T, char, string->chars, string->length + 1);
            TEA_FREE(T, TeaOString, object);
            break;
        }
        case OBJ_UPVALUE:
        {
            TEA_FREE(T, TeaOUpvalue, object);
            break;
        }
        case OBJ_NATIVE:
        {
            TEA_FREE(T, TeaONative, object);
            break;
        }
        case OBJ_USERDATA:
        {
            TeaOUserdata* ud = (TeaOUserdata*)object;
            if(ud->size > 0)
            {
                tea_mem_realloc(T, ud->data, ud->size, 0);
            }
            TEA_FREE(T, TeaOUserdata, object);
            break;
        }
    }
}

static void mark_roots(TeaState* T)
{
    for(TeaValue* slot = T->stack; slot < T->top; slot++)
    {
        tea_gc_markval(T, *slot);
    }

    for(TeaCallInfo* ci = T->base_ci; ci < T->ci; ci++)
    {
        tea_gc_markobj(T, (TeaObject*)ci->closure);
        tea_gc_markobj(T, (TeaObject*)ci->native);
    }

    for(TeaOUpvalue* upvalue = T->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        tea_gc_markobj(T, (TeaObject*)upvalue);
    }

    tea_tab_mark(T, &T->modules);
    tea_tab_mark(T, &T->globals);

    tea_gc_markobj(T, (TeaObject*)T->list_class);
    tea_gc_markobj(T, (TeaObject*)T->map_class);
    tea_gc_markobj(T, (TeaObject*)T->string_class);
    tea_gc_markobj(T, (TeaObject*)T->range_class);
    tea_gc_markobj(T, (TeaObject*)T->file_class);

    tea_gc_markobj(T, (TeaObject*)T->constructor_string);
    tea_gc_markobj(T, (TeaObject*)T->repl_string);
    tea_gc_markobj(T, (TeaObject*)T->memerr);

    for(int i = 0; i < MT_END; i++)
    {
        tea_gc_markobj(T, (TeaObject*)T->opm_name[i]);
    }

    if(T->parser != NULL)
    {
        tea_parser_mark_roots(T, T->parser);
    }
}

static void trace_references(TeaState* T)
{
    while(T->gray_count > 0)
    {
        TeaObject* object = T->gray_stack[--T->gray_count];
        blacken_object(T, object);
    }
}

static void sweep(TeaState* T)
{
    TeaObject* previous = NULL;
    TeaObject* object = T->objects;

    while(object != NULL)
    {
        if(object->is_marked)
        {
            object->is_marked = false;
            previous = object;
            object = object->next;
        }
        else
        {
            TeaObject* unreached = object;
            object = object->next;
            if(previous != NULL)
            {
                previous->next = object;
            }
            else
            {
                T->objects = object;
            }

            free_object(T, unreached);
        }
    }
}

void tea_gc_collect(TeaState* T)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = T->bytes_allocated;
#endif

    mark_roots(T);
    trace_references(T);
    tea_tab_remove_white(&T->strings);
    sweep(T);

    T->next_gc = T->bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - T->bytes_allocated, before, T->bytes_allocated, T->next_gc);
#endif
}

void tea_gc_free_objects(TeaState* T)
{
    TeaObject* object = T->objects;

    while(object != NULL)
    {
        TeaObject* next = object->next;
        free_object(T, object);
        object = next;
    }

    /* free the gray stack */
    (*T->frealloc)(T->ud, T->gray_stack, sizeof(TeaObject*) * T->gray_capacity, 0);
}