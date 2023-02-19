// tea_memory.c
// Teascript gc and memory functions

#include <stdlib.h>

#include "tea.h"

#include "tea_common.h"
#include "tea_compiler.h"
#include "tea_memory.h"
#include "tea_state.h"
#include "tea_array.h"

#ifdef TEA_DEBUG_LOG_GC
#include <stdio.h>
#include "tea_debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* tea_reallocate(TeaState* T, void* pointer, size_t old_size, size_t new_size)
{
    T->bytes_allocated += new_size - old_size;

#ifdef TEA_DEBUG_TRACE_MEMORY
    printf("total bytes allocated: %zu\nnew allocation: %zu\nold allocation: %zu\n\n", T->bytes_allocated, new_size, old_size);
#endif

    if(new_size > old_size)
    {
#ifdef TEA_DEBUG_STRESS_GC
        tea_collect_garbage(T);
#endif

        if(T->bytes_allocated > T->next_gc)
        {
            tea_collect_garbage(T);
        }
    }

    if(new_size == 0)
    {
        free(pointer);
        return NULL;
    }

    void* result = realloc(pointer, new_size);

    if(result == NULL)
        exit(1);

    return result;
}

void tea_mark_object(TeaState* T, TeaObject* object)
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
        T->gray_capacity = GROW_CAPACITY(T->gray_capacity);
        T->gray_stack = (TeaObject**)realloc(T->gray_stack, sizeof(TeaObject*) * T->gray_capacity);

        if(T->gray_stack == NULL)
            exit(1);
    }

    T->gray_stack[T->gray_count++] = object;
}

void tea_mark_value(TeaState* T, TeaValue value)
{
    if(IS_OBJECT(value))
        tea_mark_object(T, AS_OBJECT(value));
}

static void mark_array(TeaState* T, TeaValueArray* array)
{
    for(int i = 0; i < array->count; i++)
    {
        tea_mark_value(T, array->values[i]);
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
            tea_mark_object(T, (TeaObject*)file->path);
            tea_mark_object(T, (TeaObject*)file->type);
            break;
        }
        case OBJ_USERDATA:
        {
            TeaObjectUserdata* userdata = (TeaObjectUserdata*)object;
            if(userdata->fn != NULL)
            {
                userdata->fn(T, userdata, true);
            }
            break;
        }
        case OBJ_MODULE:
        {
            TeaObjectModule* module = (TeaObjectModule*)object;
            tea_mark_object(T, (TeaObject*)module->name);
            tea_mark_object(T, (TeaObject*)module->path);
            tea_mark_table(T, &module->values);
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
                tea_mark_value(T, item->key);
                tea_mark_value(T, item->value);
            }
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            TeaObjectBoundMethod* bound = (TeaObjectBoundMethod*)object;
            tea_mark_value(T, bound->receiver);
            tea_mark_value(T, bound->method);
            break;
        }
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            tea_mark_object(T, (TeaObject*)klass->name);
            tea_mark_object(T, (TeaObject*)klass->super);
            tea_mark_table(T, &klass->statics);
            tea_mark_table(T, &klass->methods);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            tea_mark_object(T, (TeaObject*)closure->function);
            if(closure->upvalues != NULL)
            {
                for(int i = 0; i < closure->upvalue_count; i++)
                {
                    tea_mark_object(T, (TeaObject*)closure->upvalues[i]);
                }
            }
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            tea_mark_object(T, (TeaObject*)function->name);
            mark_array(T, &function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            tea_mark_object(T, (TeaObject*)instance->klass);
            tea_mark_table(T, &instance->fields);
            break;
        }
        case OBJ_UPVALUE:
        {
            tea_mark_value(T, ((TeaObjectUpvalue*)object)->closed);
            break;
        }
        case OBJ_THREAD:
        {
            TeaObjectThread* thread = (TeaObjectThread*)object;

            for(TeaValue* slot = thread->stack; slot < thread->stack_top; slot++) 
            {
				tea_mark_value(T, *slot);
			}
            
            for(int i = 0; i < thread->frame_count; i++)
            {
                tea_mark_object(T, (TeaObject*)thread->frames[i].closure);
            }

            TeaObjectUpvalue* upvalue = thread->open_upvalues;
            while(upvalue != NULL)
            {
                tea_mark_object(T, (TeaObject*)upvalue);
                upvalue = upvalue->next;
            }

            tea_mark_object(T, (TeaObject*)thread->parent);
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
    printf("%p free %s\n", (void*)object, tea_object_type(OBJECT_VAL(object)));
#endif

    switch(object->type)
    {
        case OBJ_RANGE:
        {
            FREE(T, TeaObjectRange, object);
            break;
        }
        case OBJ_FILE:
        {
            TeaObjectFile* file = (TeaObjectFile*)object;
            if(file->is_open == true)
            {
                fclose(file->file);
            }
            FREE(T, TeaObjectFile, object);
            break;
        }
        case OBJ_MODULE:
        {
            TeaObjectModule* module = (TeaObjectModule*)object;
            tea_free_table(T, &module->values);
            FREE(T, TeaObjectModule, object);
            break;
        }
        case OBJ_LIST:
        {
            TeaObjectList* list = (TeaObjectList*)object;
            tea_free_value_array(T, &list->items);
            FREE(T, TeaObjectList, object);
            break;
        }
        case OBJ_MAP:
        {
            TeaObjectMap* map = (TeaObjectMap*)object;
            FREE_ARRAY(T, TeaMapItem, map->items, map->capacity);
            FREE(T, TeaObjectMap, object);
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            FREE(T, TeaObjectBoundMethod, object);
            break;
        }
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            tea_free_table(T, &klass->methods);
            tea_free_table(T, &klass->statics);
            FREE(T, TeaObjectClass, object);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            FREE_ARRAY(T, TeaObjectUpvalue *, closure->upvalues, closure->upvalue_count);
            FREE(T, TeaObjectClosure, object);
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            tea_free_chunk(T, &function->chunk);
            FREE(T, TeaObjectFunction, object);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            tea_free_table(T, &instance->fields);
            FREE(T, TeaObjectInstance, object);
            break;
        }
        case OBJ_STRING:
        {
            TeaObjectString* string = (TeaObjectString*)object;
            FREE_ARRAY(T, char, string->chars, string->length + 1);
            FREE(T, TeaObjectString, object);
            break;
        }
        case OBJ_UPVALUE:
        {
            FREE(T, TeaObjectUpvalue, object);
            break;
        }
        case OBJ_USERDATA:
        {
            TeaObjectUserdata* data = (TeaObjectUserdata*)object;

            if(data->fn != NULL)
            {
                data->fn(T, data, false);
            }

            if(data->size > 0)
            {
                tea_reallocate(T, data->data, data->size, 0);
            }

            FREE(T, TeaObjectUserdata, object);
            break;
        }
        case OBJ_THREAD:
        {
            TeaObjectThread* thread = (TeaObjectThread*)object;

            FREE_ARRAY(T, TeaCallFrame, thread->frames, thread->frame_capacity);
            FREE_ARRAY(T, TeaValue, thread->stack, thread->stack_capacity);
            FREE(T, TeaObjectThread, object);
            break;
        }
        case OBJ_NATIVE:
        {
            FREE(T, TeaObjectNative, object);
            break;
        }
    }
}

static void mark_roots(TeaState* T)
{
    for(TeaValue* slot = T->slot; slot < &T->stack[T->top]; slot++)
    {
        tea_mark_value(T, *slot);
    }

    tea_mark_table(T, &T->modules);
    tea_mark_table(T, &T->globals);
    //tea_mark_table(T, &T->constants);

    tea_mark_object(T, (TeaObject*)T->thread);

    tea_mark_object(T, (TeaObject*)T->list_class);
    tea_mark_object(T, (TeaObject*)T->map_class);
    tea_mark_object(T, (TeaObject*)T->string_class);
    tea_mark_object(T, (TeaObject*)T->range_class);
    tea_mark_object(T, (TeaObject*)T->file_class);
    
    if(T->compiler != NULL)
    {
        tea_mark_compiler_roots(T, T->compiler);
    }
    
    tea_mark_object(T, (TeaObject*)T->constructor_string);
    tea_mark_object(T, (TeaObject*)T->repl_string);
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

void tea_free_objects(TeaState* T)
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

// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float
int tea_closest_power_of_two(int n)
{
	n--;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	n++;

	return n;
}