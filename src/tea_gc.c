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
#include "tea_compiler.h"

#ifdef TEA_DEBUG_LOG_GC
#include <stdio.h>
#include "tea_debug.h"
#endif

void tea_gc_mark_object(TeaState* T, TeaObject* object)
{
    if(object == NULL)
        return;
    if(object->is_marked)
        return;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p mark %s", (void*)object, tea_value_type(OBJECT_VAL(object)));
    printf("\n");
#endif

    object->is_marked = true;

    if(T->gray_capacity < T->gray_count + 1)
    {
        T->gray_capacity = TEA_GROW_CAPACITY(T->gray_capacity);
        T->gray_stack = (TeaObject**)((*T->frealloc)(T->ud, T->gray_stack, 0, sizeof(TeaObject*) * T->gray_capacity));

        if(T->gray_stack == NULL)
            exit(1);
    }

    T->gray_stack[T->gray_count++] = object;
}

void tea_gc_mark_value(TeaState* T, TeaValue value)
{
    if(IS_OBJECT(value))
        tea_gc_mark_object(T, AS_OBJECT(value));
}

static void mark_array(TeaState* T, TeaValueArray* array)
{
    for(int i = 0; i < array->count; i++)
    {
        tea_gc_mark_value(T, array->values[i]);
    }
}

static void blacken_object(TeaState* T, TeaObject* object)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("%p blacken %s", (void*)object, tea_value_type(OBJECT_VAL(object)));
    printf("\n");
#endif

    switch(object->type)
    {
        case OBJ_FILE:
        {
            TeaObjectFile* file = (TeaObjectFile*)object;
            tea_gc_mark_object(T, (TeaObject*)file->path);
            tea_gc_mark_object(T, (TeaObject*)file->type);
            break;
        }
        case OBJ_MODULE:
        {
            TeaObjectModule* module = (TeaObjectModule*)object;
            tea_gc_mark_object(T, (TeaObject*)module->name);
            tea_gc_mark_object(T, (TeaObject*)module->path);
            tea_table_mark(T, &module->values);
            break;
        }
        case OBJ_LIST:
        {
            TeaObjectList* list = (TeaObjectList*)object;
            mark_array(T, &list->items);
            break;
        }
        case OBJ_MAP:
        {
            TeaObjectMap* map = (TeaObjectMap*)object;
            for(int i = 0; i < map->capacity; i++)
            {
                TeaMapItem* item = &map->items[i];
                tea_gc_mark_value(T, item->key);
                tea_gc_mark_value(T, item->value);
            }
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            TeaObjectBoundMethod* bound = (TeaObjectBoundMethod*)object;
            tea_gc_mark_value(T, bound->receiver);
            tea_gc_mark_value(T, bound->method);
            break;
        }
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            tea_gc_mark_object(T, (TeaObject*)klass->name);
            tea_gc_mark_object(T, (TeaObject*)klass->super);
            tea_table_mark(T, &klass->statics);
            tea_table_mark(T, &klass->methods);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            tea_gc_mark_object(T, (TeaObject*)closure->function);
            if(closure->upvalues != NULL)
            {
                for(int i = 0; i < closure->upvalue_count; i++)
                {
                    tea_gc_mark_object(T, (TeaObject*)closure->upvalues[i]);
                }
            }
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            tea_gc_mark_object(T, (TeaObject*)function->name);
            mark_array(T, &function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            tea_gc_mark_object(T, (TeaObject*)instance->klass);
            tea_table_mark(T, &instance->fields);
            break;
        }
        case OBJ_UPVALUE:
        {
            tea_gc_mark_value(T, ((TeaObjectUpvalue*)object)->closed);
            break;
        }
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
            TEA_FREE(T, TeaObjectRange, object);
            break;
        }
        case OBJ_FILE:
        {
            TeaObjectFile* file = (TeaObjectFile*)object;
            if((file->is_open == true) && file->file != NULL)
            {
                fclose(file->file);
            }
            TEA_FREE(T, TeaObjectFile, object);
            break;
        }
        case OBJ_MODULE:
        {
            TeaObjectModule* module = (TeaObjectModule*)object;
            tea_table_free(T, &module->values);
            TEA_FREE(T, TeaObjectModule, object);
            break;
        }
        case OBJ_LIST:
        {
            TeaObjectList* list = (TeaObjectList*)object;
            tea_free_value_array(T, &list->items);
            TEA_FREE(T, TeaObjectList, object);
            break;
        }
        case OBJ_MAP:
        {
            TeaObjectMap* map = (TeaObjectMap*)object;
            TEA_FREE_ARRAY(T, TeaMapItem, map->items, map->capacity);
            TEA_FREE(T, TeaObjectMap, object);
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            TEA_FREE(T, TeaObjectBoundMethod, object);
            break;
        }
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            tea_table_free(T, &klass->methods);
            tea_table_free(T, &klass->statics);
            TEA_FREE(T, TeaObjectClass, object);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            TEA_FREE_ARRAY(T, TeaObjectUpvalue *, closure->upvalues, closure->upvalue_count);
            TEA_FREE(T, TeaObjectClosure, object);
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            tea_chunk_free(T, &function->chunk);
            TEA_FREE(T, TeaObjectFunction, object);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            tea_table_free(T, &instance->fields);
            TEA_FREE(T, TeaObjectInstance, object);
            break;
        }
        case OBJ_STRING:
        {
            TeaObjectString* string = (TeaObjectString*)object;
            TEA_FREE_ARRAY(T, char, string->chars, string->length + 1);
            TEA_FREE(T, TeaObjectString, object);
            break;
        }
        case OBJ_UPVALUE:
        {
            TEA_FREE(T, TeaObjectUpvalue, object);
            break;
        }
        case OBJ_NATIVE:
        {
            TEA_FREE(T, TeaObjectNative, object);
            break;
        }
    }
}

static void mark_roots(TeaState* T)
{
    for(TeaValue* slot = T->stack; slot < T->top; slot++)
    {
        tea_gc_mark_value(T, *slot);
    }
    
    for(TeaCallInfo* ci = T->base_ci; ci < T->ci; ci++)
    {
        tea_gc_mark_object(T, (TeaObject*)ci->closure);
        tea_gc_mark_object(T, (TeaObject*)ci->native);
    }

    for(TeaObjectUpvalue* upvalue = T->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        tea_gc_mark_object(T, (TeaObject*)upvalue);
    }

    tea_table_mark(T, &T->modules);
    tea_table_mark(T, &T->globals);

    tea_gc_mark_object(T, (TeaObject*)T->list_class);
    tea_gc_mark_object(T, (TeaObject*)T->map_class);
    tea_gc_mark_object(T, (TeaObject*)T->string_class);
    tea_gc_mark_object(T, (TeaObject*)T->range_class);
    tea_gc_mark_object(T, (TeaObject*)T->file_class);
    
    tea_gc_mark_object(T, (TeaObject*)T->constructor_string);
    tea_gc_mark_object(T, (TeaObject*)T->repl_string);

    if(T->compiler != NULL)
    {
        tea_compiler_mark_roots(T, T->compiler);
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

TEA_API void tea_collect_garbage(TeaState* T)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = T->bytes_allocated;
#endif

    mark_roots(T);
    trace_references(T);
    tea_table_remove_white(&T->strings);
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

    free(T->gray_stack);
}