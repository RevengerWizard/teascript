/*
** tea_gc.c
** Teascript garbage collector
*/

#include <stdlib.h>

#include "tea_state.h"
#include "tea_memory.h"
#include "tea_gc.h"
#include "tea_compiler.h"

#ifdef TEA_DEBUG_LOG_GC
#include <stdio.h>
#include "tea_debug.h"
#endif

void teaC_mark_object(TeaState* T, TeaObject* object)
{
    if(object == NULL)
        return;
    if(object->is_marked)
        return;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p mark %s", (void*)object, teaL_type(OBJECT_VAL(object)));
    printf("\n");
#endif

    object->is_marked = true;

    if(T->gray_capacity < T->gray_count + 1)
    {
        T->gray_capacity = TEA_GROW_CAPACITY(T->gray_capacity);
        T->gray_stack = (TeaObject**)realloc(T->gray_stack, sizeof(TeaObject*) * T->gray_capacity);

        if(T->gray_stack == NULL)
            exit(1);
    }

    T->gray_stack[T->gray_count++] = object;
}

void teaC_mark_value(TeaState* T, TeaValue value)
{
    if(IS_OBJECT(value))
        teaC_mark_object(T, AS_OBJECT(value));
}

static void mark_array(TeaState* T, TeaValueArray* array)
{
    for(int i = 0; i < array->count; i++)
    {
        teaC_mark_value(T, array->values[i]);
    }
}

static void blacken_object(TeaState* T, TeaObject* object)
{
#ifdef TEA_DEBUG_LOG_GC
    printf("%p blacken %s", (void*)object, teaL_type(OBJECT_VAL(object)));
    printf("\n");
#endif

    switch(object->type)
    {
        case OBJ_FILE:
        {
            TeaObjectFile* file = (TeaObjectFile*)object;
            teaC_mark_object(T, (TeaObject*)file->path);
            teaC_mark_object(T, (TeaObject*)file->type);
            break;
        }
        case OBJ_MODULE:
        {
            TeaObjectModule* module = (TeaObjectModule*)object;
            teaC_mark_object(T, (TeaObject*)module->name);
            teaC_mark_object(T, (TeaObject*)module->path);
            teaT_mark(T, &module->values);
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
                teaC_mark_value(T, item->key);
                teaC_mark_value(T, item->value);
            }
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            TeaObjectBoundMethod* bound = (TeaObjectBoundMethod*)object;
            teaC_mark_value(T, bound->receiver);
            teaC_mark_value(T, bound->method);
            break;
        }
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            teaC_mark_object(T, (TeaObject*)klass->name);
            teaC_mark_object(T, (TeaObject*)klass->super);
            teaT_mark(T, &klass->statics);
            teaT_mark(T, &klass->methods);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            teaC_mark_object(T, (TeaObject*)closure->function);
            if(closure->upvalues != NULL)
            {
                for(int i = 0; i < closure->upvalue_count; i++)
                {
                    teaC_mark_object(T, (TeaObject*)closure->upvalues[i]);
                }
            }
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            teaC_mark_object(T, (TeaObject*)function->name);
            mark_array(T, &function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            teaC_mark_object(T, (TeaObject*)instance->klass);
            teaT_mark(T, &instance->fields);
            break;
        }
        case OBJ_UPVALUE:
        {
            teaC_mark_value(T, ((TeaObjectUpvalue*)object)->closed);
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
    printf("%p free %s\n", (void*)object, teaO_type(OBJECT_VAL(object)));
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
            teaT_free(T, &module->values);
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
            teaT_free(T, &klass->methods);
            teaT_free(T, &klass->statics);
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
            teaK_free(T, &function->chunk);
            TEA_FREE(T, TeaObjectFunction, object);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            teaT_free(T, &instance->fields);
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
        teaC_mark_value(T, *slot);
    }
    
    for(TeaCallInfo* ci = T->base_ci; ci < T->ci; ci++)
    {
        teaC_mark_object(T, (TeaObject*)ci->closure);
        teaC_mark_object(T, (TeaObject*)ci->native);
    }

    for(TeaObjectUpvalue* upvalue = T->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        teaC_mark_object(T, (TeaObject*)upvalue);
    }

    teaT_mark(T, &T->modules);
    teaT_mark(T, &T->globals);

    teaC_mark_object(T, (TeaObject*)T->list_class);
    teaC_mark_object(T, (TeaObject*)T->map_class);
    teaC_mark_object(T, (TeaObject*)T->string_class);
    teaC_mark_object(T, (TeaObject*)T->range_class);
    teaC_mark_object(T, (TeaObject*)T->file_class);
    
    teaC_mark_object(T, (TeaObject*)T->constructor_string);
    teaC_mark_object(T, (TeaObject*)T->repl_string);

    if(T->compiler != NULL)
    {
        teaY_mark_roots(T, T->compiler);
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
    teaT_remove_white(&T->strings);
    sweep(T);

    T->next_gc = T->bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef TEA_DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - T->bytes_allocated, before, T->bytes_allocated, T->next_gc);
#endif
}

void teaC_free_objects(TeaState* T)
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