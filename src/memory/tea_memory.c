#include <stdlib.h>

#include "parser/tea_parser.h"
#include "memory/tea_memory.h"
#include "vm/tea_vm.h"
#include "util/tea_array.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug/tea_debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* tea_reallocate(void* pointer, size_t old_size, size_t new_size)
{
    vm.bytes_allocated += new_size - old_size;

    if(new_size > old_size)
    {
#ifdef DEBUG_STRESS_GC
        tea_collect_garbage();
#endif

        if(vm.bytes_allocated > vm.next_GC)
        {
            tea_collect_garbage();
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

void tea_mark_object(TeaObject* object)
{
    if(object == NULL)
        return;
    if(object->is_marked)
        return;

#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *)object);
    tea_print_value(OBJECT_VAL(object));
    printf("\n");
#endif

    object->is_marked = true;

    if(vm.gray_capacity < vm.gray_count + 1)
    {
        vm.gray_capacity = GROW_CAPACITY(vm.gray_capacity);
        vm.gray_stack = (TeaObject**)realloc(vm.gray_stack, sizeof(TeaObject*) * vm.gray_capacity);

        if(vm.gray_stack == NULL)
            exit(1);
    }

    vm.gray_stack[vm.gray_count++] = object;
}

void tea_mark_value(TeaValue value)
{
    if(IS_OBJECT(value))
        tea_mark_object(AS_OBJECT(value));
}

static void mark_array(TeaValueArray* array)
{
    for(int i = 0; i < array->count; i++)
    {
        tea_mark_value(array->values[i]);
    }
}

static void blacken_object(TeaObject* object)
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
            tea_mark_object((TeaObject*)module->name);
            tea_mark_object((TeaObject*)module->path);
            tea_mark_table(&module->values);
            break;
        }
        case OBJ_LIST:
        {
            TeaObjectList* list = (TeaObjectList*)object;
            mark_array(&list->items);
            break;
        }
        case OBJ_MAP:
        {
            TeaObjectMap* map = (TeaObjectMap*)object;
            tea_mark_table(&map->items);
            break;
        }
        case OBJ_BOUND_METHOD:
        {
            TeaObjectBoundMethod* bound = (TeaObjectBoundMethod*)object;
            tea_mark_value(bound->receiver);
            tea_mark_object((TeaObject*)bound->method);
            break;
        }
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            tea_mark_object((TeaObject*)klass->name);
            tea_mark_table(&klass->methods);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            tea_mark_object((TeaObject*)closure->function);
            for(int i = 0; i < closure->upvalue_count; i++)
            {
                tea_mark_object((TeaObject *)closure->upvalues[i]);
            }
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            tea_mark_object((TeaObject*)function->name);
            mark_array(&function->chunk.constants);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            tea_mark_object((TeaObject*)instance->klass);
            tea_mark_table(&instance->fields);
            break;
        }
        case OBJ_UPVALUE:
            tea_mark_value(((TeaObjectUpvalue*)object)->closed);
            break;
        case OBJ_NATIVE:
        case OBJ_STRING:
        case OBJ_RANGE:
        case OBJ_FILE:
            break;
    }
}

static void free_object(TeaObject* object)
{
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void *)object, object->type);
#endif

    switch(object->type)
    {
        case OBJ_RANGE:
        {
            FREE(TeaObjectRange, object);
            break;
        }
        case OBJ_FILE:
        {
            FREE(TeaObjectFile, object);
            break;
        }
        case OBJ_MODULE:
        {
            TeaObjectModule* module = (TeaObjectModule*)object;
            tea_free_table(&module->values);
            FREE(TeaObjectModule, object);
            break;
        }
        case OBJ_LIST:
        {
            TeaObjectList* list = (TeaObjectList*)object;
            tea_free_value_array(&list->items);
            FREE(TeaObjectList, object);
            break;
        }
        case OBJ_MAP:
        {
            TeaObjectMap* map = (TeaObjectMap*)object;
            tea_free_table(&map->items);
            FREE(TeaObjectMap, object);
            break;
        }
        case OBJ_BOUND_METHOD:
            FREE(TeaObjectBoundMethod, object);
            break;
        case OBJ_CLASS:
        {
            TeaObjectClass* klass = (TeaObjectClass*)object;
            tea_free_table(&klass->methods);
            FREE(TeaObjectClass, object);
            break;
        }
        case OBJ_CLOSURE:
        {
            TeaObjectClosure* closure = (TeaObjectClosure*)object;
            FREE_ARRAY(TeaObjectUpvalue *, closure->upvalues, closure->upvalue_count);
            FREE(TeaObjectClosure, object);
            break;
        }
        case OBJ_FUNCTION:
        {
            TeaObjectFunction* function = (TeaObjectFunction*)object;
            tea_free_chunk(&function->chunk);
            FREE(TeaObjectFunction, object);
            break;
        }
        case OBJ_INSTANCE:
        {
            TeaObjectInstance* instance = (TeaObjectInstance*)object;
            tea_free_table(&instance->fields);
            FREE(TeaObjectInstance, object);
            break;
        }
        case OBJ_NATIVE:
        {
            FREE(TeaObjectNative, object);
            break;
        }
        case OBJ_STRING:
        {
            TeaObjectString* string = (TeaObjectString*)object;
            FREE_ARRAY(char, string->chars, string->length + 1);
            FREE(TeaObjectString, object);
            break;
        }
        case OBJ_UPVALUE:
        {
            FREE(TeaObjectUpvalue, object);
            break;
        }
    }
}

static void mark_roots()
{
    for(TeaValue* slot = vm.stack; slot < vm.stack_top; slot++)
    {
        tea_mark_value(*slot);
    }

    for(int i = 0; i < vm.frame_count; i++)
    {
        tea_mark_object((TeaObject*)vm.frames[i].closure);
    }

    for(TeaObjectUpvalue* upvalue = vm.open_upvalues; upvalue != NULL; upvalue = upvalue->next)
    {
        tea_mark_object((TeaObject*)upvalue);
    }

    tea_mark_table(&vm.globals);
    tea_mark_compiler_roots();
    tea_mark_object((TeaObject*)vm.init_string);
}

static void trace_references()
{
    while(vm.gray_count > 0)
    {
        TeaObject* object = vm.gray_stack[--vm.gray_count];
        blacken_object(object);
    }
}

static void sweep()
{
    TeaObject* previous = NULL;
    TeaObject* object = vm.objects;
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
                vm.objects = object;
            }

            free_object(unreached);
        }
    }
}

void tea_collect_garbage()
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytes_allocated;
#endif

    mark_roots();
    trace_references();
    tea_table_remove_white(&vm.strings);
    sweep();

    vm.next_GC = vm.bytes_allocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %zu bytes (from %zu to %zu) next at %zu\n", before - vm.bytes_allocated, before, vm.bytes_allocated, vm.next_GC);
#endif
}

void tea_free_objects()
{
    TeaObject* object = vm.objects;
    while(object != NULL)
    {
        TeaObject* next = object->next;
        free_object(object);
        object = next;
    }

    free(vm.gray_stack);
}