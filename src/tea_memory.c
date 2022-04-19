#include <stdlib.h>

#include "tea_compiler.h"
#include "tea_memory.h"
#include "tea_vm.h"
#include "tea_array.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug/tea_debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* tea_reallocate(TeaState* state, void* pointer, size_t old_size, size_t new_size)
{
    state->bytes_allocated += new_size - old_size;

#ifdef DEBUG_TRACE_MEMORY
    printf("total bytes allocated: %zu\nnew allocation: %zu\nold allocation: %zu\n\n", state->bytes_allocated, new_size, old_size);
#endif

    if(new_size > old_size)
    {
#ifdef DEBUG_STRESS_GC
        tea_collect_garbage(state->vm);
#endif

        if(state->bytes_allocated > state->next_gc)
        {
            tea_collect_garbage(state->vm);
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

void tea_mark_object(TeaVM* vm, TeaObject* object)
{
    if(object == NULL)
        return;
    if(object->is_marked)
        return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void*)object);
    tea_print_value(OBJECT_VAL(object));
    printf("\n");
#endif

    object->is_marked = true;

    if(vm->gray_capacity < vm->gray_count + 1)
    {
        vm->gray_capacity = GROW_CAPACITY(vm->gray_capacity);
        vm->gray_stack = (TeaObject**)realloc(vm->gray_stack, sizeof(TeaObject*) * vm->gray_capacity);

        if(vm->gray_stack == NULL)
            exit(1);
    }

    vm->gray_stack[vm->gray_count++] = object;
}

void tea_mark_value(TeaVM* vm, TeaValue value)
{
    if(IS_OBJECT(value))
        tea_mark_object(vm, AS_OBJECT(value));
}

static void mark_array(TeaVM* vm, TeaValueArray* array)
{
    for(int i = 0; i < array->count; i++)
    {
        tea_mark_value(vm, array->values[i]);
    }
}

static void blacken_object(TeaVM* vm, TeaObject* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void*)object);
    tea_print_value(OBJECT_VAL(object));
    printf("\n");
#endif

    switch(object->type)
    {
        case OBJ_MODULE:
        {
            TeaObjectModule* module = (TeaObjectModule*)object;
            tea_mark_object(vm, (TeaObject*)module->name);
            tea_mark_object(vm, (TeaObject*)module->path);
            tea_mark_table(vm, &module->values);
            break;
        }
        case OBJ_LIST:
        {
            TeaObjectList* list = (TeaObjectList*)object;
            mark_array(vm, &list->items);
            break;
        }
        case OBJ_MAP:
        {
            TeaObjectMap* map = (TeaObjectMap*)object;
            tea_mark_table(vm, &map->items);
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            TeaObjectBoundMethod* bound = (TeaObjectBoundMethod*)object;
            tea_mark_value(vm, bound->receiver);
            tea_mark_object(vm, (TeaObject*)bound->method);
            break;
        }
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            tea_mark_object(vm, (TeaObject*)klass->name);
            tea_mark_table(vm, &klass->methods);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            tea_mark_object(vm, (TeaObject*)closure->function);
            for(int i = 0; i < closure->upvalue_count; i++)
            {
                tea_mark_object(vm, (TeaObject *)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            tea_mark_object(vm, (TeaObject*)function->name);
            mark_array(vm, &function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            tea_mark_object(vm, (TeaObject*)instance->klass);
            tea_mark_table(vm, &instance->fields);
            break;
        }
        case OBJ_UPVALUE:
            tea_mark_value(vm, ((TeaObjectUpvalue*)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
        case OBJ_RANGE:
        case OBJ_FILE:
            break;
    }
}

static void free_object(TeaState* state, TeaObject* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*)object, object->type);
#endif

    switch(object->type)
    {
        case OBJ_RANGE:
        {
            FREE(state, TeaObjectRange, object);
            break;
        }
        case OBJ_FILE:
        {
            FREE(state, TeaObjectFile, object);
            break;
        }
        case OBJ_MODULE:
        {
            TeaObjectModule* module = (TeaObjectModule*)object;
            tea_free_table(state, &module->values);
            FREE(state, TeaObjectModule, object);
            break;
        }
        case OBJ_LIST:
        {
            TeaObjectList* list = (TeaObjectList*)object;
            tea_free_value_array(state, &list->items);
            FREE(state, TeaObjectList, object);
            break;
        }
        case OBJ_MAP:
        {
            TeaObjectMap* map = (TeaObjectMap*)object;
            tea_free_table(state, &map->items);
            FREE(state, TeaObjectMap, object);
            break;
        }
        case OBJ_BOUND_METHOD:
            FREE(state, TeaObjectBoundMethod, object);
            break;
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            tea_free_table(state, &klass->methods);
            FREE(state, TeaObjectClass, object);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            FREE_ARRAY(state, TeaObjectUpvalue *, closure->upvalues, closure->upvalue_count);
            FREE(state, TeaObjectClosure, object);
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            tea_free_chunk(state, &function->chunk);
            FREE(state, TeaObjectFunction, object);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            tea_free_table(state, &instance->fields);
            FREE(state, TeaObjectInstance, object);
            break;
        }
        case OBJ_NATIVE:
        {
            FREE(state, TeaObjectNative, object);
            break;
        }
        case OBJ_STRING:
        {
            TeaObjectString* string = (TeaObjectString*)object;
            FREE_ARRAY(state, char, string->chars, string->length + 1);
            FREE(state, TeaObjectString, object);
            break;
        }
        case OBJ_UPVALUE:
        {
            FREE(state, TeaObjectUpvalue, object);
            break;
        }
    }
}

static void mark_roots(TeaVM* vm)
{
    for(TeaValue* slot = vm->stack; slot < vm->stack_top; slot++)
    {
        tea_mark_value(vm, *slot);
    }

    for(int i = 0; i < vm->frame_count; i++)
    {
        tea_mark_object(vm, (TeaObject*)vm->frames[i].closure);
    }

    for(TeaObjectUpvalue* upvalue = vm->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        tea_mark_object(vm, (TeaObject*)upvalue);
    }

    tea_mark_table(vm, &vm->globals);
    tea_mark_compiler_roots(vm->state);
}

static void trace_references(TeaVM* vm)
{
    while(vm->gray_count > 0)
    {
        TeaObject* object = vm->gray_stack[--vm->gray_count];
        blacken_object(vm, object);
    }
}

static void sweep(TeaVM* vm)
{
    TeaObject* previous = NULL;
    TeaObject* object = vm->objects;

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
                vm->objects = object;
            }

            free_object(vm->state, unreached);
        }
    }
}

void tea_collect_garbage(TeaVM* vm)
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm->state->bytes_allocated;
#endif

    mark_roots(vm);
    trace_references(vm);
    tea_table_remove_white(&vm->strings);
    sweep(vm);

    vm->state->next_gc = vm->state->bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - state->vm->bytes_allocated, before, state->vm->bytes_allocated, vm.next_GC);
#endif
}

void tea_free_objects(TeaState* state, TeaObject* objects)
{
    TeaObject* object = objects;

    while(object != NULL)
    {
        TeaObject* next = object->next;
        free_object(state, object);
        object = next;
    }

    free(state->vm->gray_stack);
}