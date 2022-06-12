/* 
** tea_vm.c
** Teascript virtual machine
*/ 

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tea_common.h"
#include "tea_compiler.h"
#include "tea_debug.h"
#include "tea_object.h"
#include "tea_memory.h"
#include "tea_vm.h"
#include "tea_util.h"

#include "tea_utf.h"
#include "tea_core.h"
#include "tea_module.h"

static void reset_stack(TeaVM* vm)
{
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
}

void tea_runtime_error(TeaVM* vm, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for(int i = vm->frame_count - 1; i >= 0; i--)
    {
        TeaCallFrame* frame = &vm->frames[i];
        TeaObjectFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", tea_get_line(&function->chunk, instruction));
        if(function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    reset_stack(vm);

    vm->error = true;
}

void tea_init_vm(TeaState* state, TeaVM* vm)
{
    reset_stack(vm);

    vm->state = state;
    vm->objects = NULL;

    vm->gray_count = 0;
    vm->gray_capacity = 0;
    vm->gray_stack = NULL;

    vm->error = false;

    vm->last_module = NULL;
    tea_init_table(&vm->modules);
    tea_init_table(&vm->globals);
    tea_init_table(&vm->strings);

    tea_init_table(&vm->string_methods);
    tea_init_table(&vm->list_methods);
    tea_init_table(&vm->map_methods);
    tea_init_table(&vm->file_methods);
    tea_init_table(&vm->range_methods);

    tea_open_core(vm);
}

void tea_free_vm(TeaVM* vm)
{
    tea_free_table(vm->state, &vm->modules);
    tea_free_table(vm->state, &vm->globals);
    tea_free_table(vm->state, &vm->strings);

    tea_free_objects(vm->state, vm->objects);

    tea_free_table(vm->state, &vm->string_methods);
    tea_free_table(vm->state, &vm->list_methods);
    tea_free_table(vm->state, &vm->map_methods);
    tea_free_table(vm->state, &vm->file_methods);
    tea_free_table(vm->state, &vm->range_methods);

#ifdef DEBUG_TRACE_MEMORY
    printf("total bytes lost: %zu\n", vm->state->bytes_allocated);
#endif
}

static bool in_(TeaVM* vm, TeaValue object, TeaValue value)
{
    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_STRING:
            {
                if(!IS_STRING(value))
                {
                    tea_push(vm, FALSE_VAL);
                    return true;
                }

                TeaObjectString* string = AS_STRING(object);
                TeaObjectString* sub = AS_STRING(value);

                if(sub == string)
                {
                    tea_push(vm, TRUE_VAL);
                    return true;
                }

                tea_push(vm, BOOL_VAL(strstr(string->chars, sub->chars) != NULL));
                return true;
            }
            case OBJ_RANGE:
            {
                if(!IS_NUMBER(value))
                {
                    tea_push(vm, FALSE_VAL);
                    return true;
                }

                double number = AS_NUMBER(value);
                TeaObjectRange* range = AS_RANGE(object);
                int from = range->from;
                int to = !range->inclusive ? range->to - 1 : range->to;

                if(number < from || number > to)
                {
                    tea_push(vm, FALSE_VAL);
                    return true;
                }

                tea_push(vm, TRUE_VAL);
                return true;
            }
            case OBJ_LIST:
            {
                TeaObjectList* list = AS_LIST(object);

                for(int i = 0; i < list->items.count; ++i) 
                {
                    if(tea_values_equal(list->items.values[i], value)) 
                    {
                        tea_push(vm, TRUE_VAL);
                        return true;
                    }
                }

                tea_push(vm, FALSE_VAL);
                return true;
            }
            default:
                break;
        }
    }

    tea_runtime_error(vm, "%s is not an iterable", tea_value_type(object));
    return false;
}

static bool subscript(TeaVM* vm, TeaValue index_value, TeaValue subscript_value)
{
    if(IS_OBJECT(subscript_value))
    {
        switch(OBJECT_TYPE(subscript_value))
        {
            case OBJ_LIST:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error(vm, "List index must be a number");
                    return false;
                }

                TeaObjectList* list = AS_LIST(subscript_value);
                int index = AS_NUMBER(index_value);

                // Allow negative indexes
                if(index < 0)
                {
                    index = list->items.count + index;
                }

                if(index >= 0 && index < list->items.count) 
                {
                    tea_pop(vm);
                    tea_pop(vm);
                    tea_push(vm, list->items.values[index]);
                    return true;
                }

                tea_runtime_error(vm, "List index out of bounds");
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(subscript_value);
                if(!tea_is_valid_key(index_value))
                {
                    tea_runtime_error(vm, "Map key isn't hashable");
                    return false;
                }

                TeaValue value;
                tea_pop(vm);
                tea_pop(vm);
                if(tea_map_get(map, index_value, &value))
                {
                    tea_push(vm, value);
                    return true;
                }

                tea_runtime_error(vm, "Key does not exist within map");
                return false;
            }
            case OBJ_STRING:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error(vm, "String index must be a number (got %s)", tea_value_type(index_value));
                    return false;
                }

                TeaObjectString* string = AS_STRING(subscript_value);
                int index = AS_NUMBER(index_value);
                int real_length = tea_ustring_length(string);

                // Allow negative indexes
                if(index < 0)
                {
                    index = real_length + index;
                }

                if(index >= 0 && index < string->length)
                {
                    tea_pop(vm);
                    tea_pop(vm);
                    TeaObjectString* c = tea_ustring_code_point_at(vm->state, string, tea_uchar_offset(string->chars, index));
                    tea_push(vm, c == NULL ? NULL_VAL : OBJECT_VAL(c));
                    return true;
                }

                tea_runtime_error(vm, "String index out of bounds");
                return false;
            }
            default:
                break;
        }
    }
    
    tea_runtime_error(vm, "%s not subscriptable", tea_value_type(subscript_value));
    return false;
}

static bool subscript_store(TeaVM* vm, TeaValue item_value, TeaValue index_value, TeaValue subscript_value, bool assign)
{
    if(IS_OBJECT(subscript_value))
    {
        switch(OBJECT_TYPE(subscript_value))
        {
            case OBJ_LIST:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error(vm, "List index must be a number (got %s)", tea_value_type(index_value));
                    return false;
                }

                TeaObjectList* list = AS_LIST(subscript_value);
                int index = AS_NUMBER(index_value);

                if(index < 0)
                {
                    index = list->items.count + index;
                }

                if(index >= 0 && index < list->items.count) 
                {
                    if(assign)
                    {
                        list->items.values[index] = item_value;
                        tea_pop(vm);
                        tea_pop(vm);
                        tea_pop(vm);
                        tea_push(vm, EMPTY_VAL);
                    }
                    else
                    {
                        vm->stack_top[-1] = list->items.values[index];
                        tea_push(vm, item_value);
                    }
                    return true;
                }

                tea_runtime_error(vm, "List index out of bounds");
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(subscript_value);
                if(!tea_is_valid_key(index_value))
                {
                    tea_runtime_error(vm, "Map key isn't hashable");
                    return false;
                }

                if(assign)
                {
                    tea_map_set(vm->state, map, index_value, item_value);
                    tea_pop(vm);
                    tea_pop(vm);
                    tea_pop(vm);
                    tea_push(vm, EMPTY_VAL);
                }
                else
                {
                    TeaValue map_value;
                    if(!tea_map_get(map, index_value, &map_value))
                    {
                        tea_runtime_error(vm, "Key does not exist within the map");
                        return false;
                    }
                    vm->stack_top[-1] = map_value;
                    tea_push(vm, item_value);
                }
                
                return true;
            }
            default:
                break;
        }
    }

    tea_runtime_error(vm, "%s does not support item assignment", tea_value_type(subscript_value));
    return false;
}

static bool slice(TeaVM* vm, TeaValue object, TeaValue start, TeaValue end)
{
    if(!IS_OBJECT(object))
    {
        tea_runtime_error(vm, "Can only slice on lists and strings");
        return false;
    }

    if((!IS_NUMBER(start) && !IS_NULL(start)) || (!IS_NUMBER(end) && !IS_NULL(end)))
    {
        tea_runtime_error(vm, "Slice index must be a number");
        return false;
    }

    int index_start;
    int index_end;
    TeaValue return_value;

    if(IS_NULL(start))
    {
        index_start = 0;
    }
    else
    {
        index_start = AS_NUMBER(start);

        if(index_start < 0)
        {
            index_start = 0;
        }
    }

    switch(OBJECT_TYPE(object))
    {
        case OBJ_LIST:
        {
            TeaObjectList* new_list = tea_new_list(vm->state);
            tea_push(vm, OBJECT_VAL(new_list));
            TeaObjectList* list = AS_LIST(object);

            if(IS_NULL(end))
            {
                index_end = list->items.count;
            }
            else
            {
                index_end = AS_NUMBER(end);

                if(index_end > list->items.count)
                {
                    index_end = list->items.count;
                }
                else if(index_end < 0)
                {
                    index_end = list->items.count + index_end;
                }
            }

            for(int i = index_start; i < index_end; i++)
            {
                tea_write_value_array(vm->state, &new_list->items, list->items.values[i]);
            }

            tea_pop(vm);
            return_value = OBJECT_VAL(new_list);

            break;
        }
        case OBJ_STRING:
        {
            TeaObjectString* string = AS_STRING(object);
            int length = tea_ustring_length(string);

            if(IS_NULL(end)) 
            {
                index_end = length;
            } 
            else 
            {
                index_end = AS_NUMBER(end);

                if(index_end > length) 
                {
                    index_end = length;
                }
                else if(index_end < 0) 
                {
                    index_end = length + index_end;
                }
            }

            // Ensure the start index is below the end index
            if(index_start > index_end) 
            {
                return_value = OBJECT_VAL(tea_copy_string(vm->state, "", 0));
            } 
            else 
            {
                index_start = tea_uchar_offset(string->chars, index_start);
	            index_end = tea_uchar_offset(string->chars, index_end);
                return_value = OBJECT_VAL(tea_ustring_from_range(vm->state, string, index_start, index_end - index_start));
            }
            break;
        }
        default:
        {
            tea_runtime_error(vm, "Can only slice lists and strings");
            return false;
        }
    }

    tea_pop(vm);
    tea_pop(vm);
    tea_pop(vm);
    tea_push(vm, return_value);
    return true;
}

static bool call(TeaVM* vm, TeaObjectClosure* closure, int count)
{
    if(count != closure->function->arity)
    {
        tea_runtime_error(vm, "Expected %d arguments but got %d", closure->function->arity, count);
        return false;
    }

    if(vm->frame_count == FRAMES_MAX)
    {
        tea_runtime_error(vm, "Stack overflow");
        return false;
    }

    TeaCallFrame* frame = &vm->frames[vm->frame_count++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;
    frame->slots = vm->stack_top - count - 1;

    return true;
}

static bool call_value(TeaVM* vm, TeaValue callee, uint8_t count)
{
    if(IS_OBJECT(callee))
    {
        switch(OBJECT_TYPE(callee))
        {
            case OBJ_BOUND_METHOD:
            {
                TeaObjectBoundMethod* bound = AS_BOUND_METHOD(callee);

                vm->stack_top[-count - 1] = bound->receiver;
                return call(vm, bound->method, count);
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(callee);
                
                vm->stack_top[-count - 1] = OBJECT_VAL(tea_new_instance(vm->state, klass));
                if(!IS_NULL(klass->constructor)) 
                {
                    return call(vm, AS_CLOSURE(klass->constructor), count);
                }
                else if(count != 0)
                {
                    tea_runtime_error(vm, "Expected 0 arguments but got %d", count);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return call(vm, AS_CLOSURE(callee), count);
            case OBJ_NATIVE_FUNCTION:
            {
                TeaNativeFunction native = AS_NATIVE_FUNCTION(callee);
                TeaValue result = native(vm, count, vm->stack_top - count);

                if(vm->error)
                {
                    vm->error = false;
                    return false;
                }
                
                vm->stack_top -= count + 1;
                tea_push(vm, result);
                return true;
            }
            case OBJ_NATIVE_METHOD:
            {
                TeaNativeMethod method = AS_NATIVE_METHOD(callee);
                TeaValue result = method(vm, *(vm->stack_top - count - 1), count, vm->stack_top - count);

                if(vm->error)
                {
                    vm->error = false;
                    return false;
                }
                
                vm->stack_top -= count + 1;
                tea_push(vm, result);
                return true;
            }
            default:
                break; // Non-callable object type
        }
    }

    tea_runtime_error(vm, "Not callable");
    return false;
}

static bool invoke_from_class(TeaVM* vm, TeaObjectClass* klass, TeaObjectString* name, int count)
{
    TeaValue method;
    if(!tea_table_get(&klass->methods, name, &method))
    {
        tea_runtime_error(vm, "Undefined property '%s'", name->chars);
        return false;
    }

    return call(vm, AS_CLOSURE(method), count);
}

static bool invoke(TeaVM* vm, TeaValue receiver, TeaObjectString* name, int count)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_FILE:
            {
                TeaValue value;
                if(tea_table_get(&vm->file_methods, name, &value)) 
                {
                    return call_value(vm, value, count);
                }

                tea_runtime_error(vm, "file has no method %s()", name->chars);
                return false;
            }
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(receiver);

                TeaValue value;
                if(!tea_table_get(&module->values, name, &value)) 
                {
                    tea_runtime_error(vm, "Undefined property '%s' in '%s' module", name->chars, module->name->chars);
                    return false;
                }

                return call_value(vm, value, count);
            }
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);

                TeaValue value;
                if(tea_table_get(&instance->klass->methods, name, &value)) 
                {
                    return call(vm, AS_CLOSURE(value), count);
                }

                if(tea_table_get(&instance->fields, name, &value))
                {
                    if(IS_NATIVE_METHOD(value))
                    {
                        return call_value(vm, value, count);
                    }
                    vm->stack_top[-count - 1] = value;
                    return call_value(vm, value, count);
                }

                tea_runtime_error(vm, "Undefined property '%s'", name->chars);
                return false;
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(receiver);
                TeaValue method;
                if(tea_table_get(&klass->methods, name, &method)) 
                {
                    if(AS_CLOSURE(method)->function->type != TYPE_STATIC) 
                    {
                        tea_runtime_error(vm, "'%s' is not static. Only static methods can be invoked directly from a class", name->chars);
                        return false;
                    }

                    return call_value(vm, method, count);
                }

                tea_runtime_error(vm, "Undefined property '%s'.", name->chars);
                return false;
            }
            case OBJ_STRING:
            {
                TeaValue value;
                if(tea_table_get(&vm->string_methods, name, &value)) 
                {
                    return call_value(vm, value, count);
                }

                tea_runtime_error(vm, "string has no method %s()", name->chars);
                return false;
            }
            case OBJ_RANGE:
            {
                TeaValue value;
                if(tea_table_get(&vm->range_methods, name, &value)) 
                {
                    return call_value(vm, value, count);
                }

                tea_runtime_error(vm, "range has no method %s()", name->chars);
                return false;
            }
            case OBJ_LIST:
            {
                TeaValue value;
                if(tea_table_get(&vm->list_methods, name, &value)) 
                {
                    if(IS_NATIVE_FUNCTION(value) || IS_NATIVE_METHOD(value)) 
                    {
                        return call_value(vm, value, count);
                    }

                    tea_push(vm, tea_peek(vm, 0));

                    for(int i = 2; i <= count + 1; i++) 
                    {
                        vm->stack_top[-i] = tea_peek(vm, i);
                    }

                    return call(vm, AS_CLOSURE(value), count + 1);
                }

                tea_runtime_error(vm, "list has no method %s()", name->chars);
                return false;
            }
            case OBJ_MAP:
            {
                TeaValue value;
                if(tea_table_get(&vm->map_methods, name, &value))
                {
                    if(IS_NATIVE_FUNCTION(value) || IS_NATIVE_METHOD(value))
                    {
                        return call_value(vm, value, count);
                    }

                    tea_push(vm, tea_peek(vm, 0));

                    for (int i = 2; i <= count + 1; i++)
                    {
                        vm->stack_top[-i] = tea_peek(vm, i);
                    }

                    return call(vm, AS_CLOSURE(value), count + 1);
                }

                tea_runtime_error(vm, "map has no method %s().", name->chars);
                return false;
            }
            default:
                break;
        }
    }

    if(strcmp(name->chars, "iterate") == 0)
    {
        tea_runtime_error(vm, "%s is not an iterator", tea_value_type(receiver));
        return false;
    }
    
    tea_runtime_error(vm, "Only objects have methods, %s given", tea_value_type(receiver));
    return false;
}

static bool bind_method(TeaVM* vm, TeaObjectClass* klass, TeaObjectString* name)
{
    TeaValue method;
    if(!tea_table_get(&klass->methods, name, &method))
    {
        tea_runtime_error(vm, "Undefined property '%s'", name->chars);
        return false;
    }

    TeaObjectBoundMethod* bound = tea_new_bound_method(vm->state, tea_peek(vm, 0), AS_CLOSURE(method));
    tea_pop(vm);
    tea_push(vm, OBJECT_VAL(bound));
    return true;
}

static bool get_property(TeaVM* vm, TeaValue receiver, TeaObjectString* name, bool pop)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_LIST:
            {
                TeaObjectList* list = AS_LIST(receiver);
                TeaValue value;

                if(tea_table_get(&vm->list_methods, name, &value)) 
                {
                    if(IS_NATIVE_PROPERTY(value))
                    {
                        TeaNativeProperty property = AS_NATIVE_PROPERTY(value);
                        TeaValue result = property(vm, OBJECT_VAL(list));
                        tea_pop(vm);
                        tea_push(vm, result);
                        return true;
                    }
                }

                tea_runtime_error(vm, "list has no property: '%s'", name->chars);
                return false;
            }
            case OBJ_STRING:
            {
                TeaObjectString* string = AS_STRING(receiver);
                TeaValue value;

                if(tea_table_get(&vm->string_methods, name, &value)) 
                {
                    if(IS_NATIVE_PROPERTY(value))
                    {
                        TeaNativeProperty property = AS_NATIVE_PROPERTY(value);
                        TeaValue result = property(vm, OBJECT_VAL(string));
                        tea_pop(vm);
                        tea_push(vm, result);
                        return true;
                    }
                }

                tea_runtime_error(vm, "string has no property: '%s'", name->chars);
                return false;
            }
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);
                
                TeaValue value;
                if(tea_table_get(&instance->fields, name, &value))
                {
                    if(IS_NATIVE_PROPERTY(value))
                    {
                        TeaNativeProperty property = AS_NATIVE_PROPERTY(value);
                        TeaValue result = property(vm, OBJECT_VAL(instance));
                        tea_pop(vm);
                        tea_push(vm, result);
                        return true;
                    }

                    if(pop)
                    {
                        tea_pop(vm); // Instance.
                    }
                    tea_push(vm, value);
                    return true;
                }

                TeaObjectClass* klass = instance->klass;
                while(klass != NULL) 
                {
                    if(tea_table_get(&klass->statics, name, &value))
                    {
                        if(pop)
                        {
                            tea_pop(vm); // Instance.
                        }
                        tea_push(vm, value);
                        return true;
                    }

                    klass = klass->super;
                }

                if(!bind_method(vm, instance->klass, name))
                {
                    return false;
                }

                tea_runtime_error(vm, "'%s' instance has no property: '%s'", instance->klass->name->chars, name->chars);
                return false;
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(receiver);
                TeaObjectClass* klass_store = klass;

                while(klass != NULL) 
                {
                    TeaValue value;
                    if(tea_table_get(&klass->statics, name, &value))
                    {
                        if(pop)
                        {
                            tea_pop(vm); // Class.
                        }
                        tea_push(vm, value);
                        return true;
                    }

                    klass = klass->super;
                }

                tea_runtime_error(vm, "'%s' class has no property: '%s'.", klass_store->name->chars, name->chars);
                return false;
            }
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(receiver);
                TeaValue value;

                if(tea_table_get(&module->values, name, &value)) 
                {
                    tea_pop(vm); // Module.
                    tea_push(vm, value);
                    return true;
                }

                tea_runtime_error(vm, "'%s' module has no property: '%s'", module->name->chars, name->chars);
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(receiver);
                TeaValue value;

                if(tea_table_get(&vm->map_methods, name, &value)) 
                {
                    if(IS_NATIVE_PROPERTY(value))
                    {
                        TeaNativeProperty property = AS_NATIVE_PROPERTY(value);
                        TeaValue result = property(vm, OBJECT_VAL(map));
                        tea_pop(vm);
                        tea_push(vm, result);
                        return true;
                    }
                }

                if(tea_map_get(map, OBJECT_VAL(name), &value))
                {
                    if(pop)
                    {
                        tea_pop(vm);
                    }
                    tea_push(vm, value);
                    return true;
                }
                
                tea_runtime_error(vm, "Map has no property");
                return false;
            }
            case OBJ_FILE:
            {
                TeaObjectFile* file = AS_FILE(receiver);
                TeaValue value;

                if(tea_table_get(&vm->file_methods, name, &value)) 
                {
                    if(IS_NATIVE_PROPERTY(value))
                    {
                        TeaNativeProperty property = AS_NATIVE_PROPERTY(value);
                        TeaValue result = property(vm, OBJECT_VAL(file));
                        tea_pop(vm);
                        tea_push(vm, result);
                        return true;
                    }
                }

                tea_runtime_error(vm, "File has no property");
                return false;
            }
            default:
                break;
        }
    }

    tea_runtime_error(vm, "Only objects have properties");
    return false;
}

static bool set_property(TeaVM* vm, TeaObjectString* name, TeaValue receiver)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);
                tea_table_set(vm->state, &instance->fields, name, tea_peek(vm, 0));
                tea_pop(vm);
                tea_pop(vm);
                tea_push(vm, EMPTY_VAL);
                return true;
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(receiver);
                tea_table_set(vm->state, &klass->statics, name, tea_peek(vm, 0));
                tea_pop(vm);
                tea_pop(vm);
                tea_push(vm, EMPTY_VAL);
                return true;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(receiver);
                tea_map_set(vm->state, map, OBJECT_VAL(name), tea_peek(vm, 0));
                tea_pop(vm);
                tea_pop(vm);
                tea_push(vm, EMPTY_VAL);
                return true;
            }
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(receiver);
                tea_table_set(vm->state, &module->values, name, tea_peek(vm, 0));
                tea_pop(vm);
                tea_pop(vm);
                tea_push(vm, EMPTY_VAL);
                return true;
            }
            default:
                break;
        }
    }

    tea_runtime_error(vm, "Can not set property on type");
    return false;
}

static TeaObjectUpvalue* capture_upvalue(TeaVM* vm, TeaValue* local)
{
    TeaObjectUpvalue* prev_upvalue = NULL;
    TeaObjectUpvalue* upvalue = vm->open_upvalues;
    while(upvalue != NULL && upvalue->location > local)
    {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    TeaObjectUpvalue* created_upvalue = tea_new_upvalue(vm->state, local);
    created_upvalue->next = upvalue;

    if(prev_upvalue == NULL)
    {
        vm->open_upvalues = created_upvalue;
    }
    else
    {
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

static void close_upvalues(TeaVM* vm, TeaValue* last)
{
    while(vm->open_upvalues != NULL && vm->open_upvalues->location >= last)
    {
        TeaObjectUpvalue* upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}

static void define_method(TeaVM* vm, TeaObjectString* name)
{
    TeaValue method = tea_peek(vm, 0);
    TeaObjectClass* klass = AS_CLASS(tea_peek(vm, 1));
    TeaObjectString* constructor_string = tea_copy_string(vm->state, "constructor", 11);
    tea_table_set(vm->state, &klass->methods, name, method);
    if(name == constructor_string) klass->constructor = method;
    tea_pop(vm);
}

static void concatenate(TeaVM* vm)
{
    TeaObjectString* b = AS_STRING(tea_peek(vm, 0));
    TeaObjectString* a = AS_STRING(tea_peek(vm, 1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(vm->state, char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    TeaObjectString* result = tea_take_string(vm->state, chars, length);
    tea_pop(vm);
    tea_pop(vm);
    tea_push(vm, OBJECT_VAL(result));
}

static TeaInterpretResult run_interpreter(TeaState* state)
{
    register TeaVM* vm = state->vm;

    TeaCallFrame* frame = &vm->frames[vm->frame_count - 1];

    register uint8_t* ip = frame->ip;

#define PUSH(value) (*vm->stack_top++ = value)
#define POP() (*(--vm->stack_top))
#define PEEK(distance) vm->stack_top[-1 - (distance)]
#define DROP() (vm->stack_top--)
#define DROP_MULTIPLE(amount) (vm->stack_top -= amount)
#define STORE_FRAME (frame->ip = ip)
#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(value_type, op, type) \
    do \
    { \
        if(!IS_NUMBER(PEEK(0)) || !IS_NUMBER(PEEK(1))) \
        { \
            STORE_FRAME; \
            tea_runtime_error(vm, "Operands must be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        type b = AS_NUMBER(POP()); \
        type a = AS_NUMBER(PEEK(0)); \
        vm->stack_top[-1] = value_type(a op b); \
    } \
    while(false)

#define BINARY_OP_FUNCTION(value_type, func, type) \
    do \
    { \
        if(!IS_NUMBER(PEEK(0)) || !IS_NUMBER(PEEK(1))) \
        { \
            STORE_FRAME; \
            tea_runtime_error(vm, "Operands must be numbers"); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        type b = AS_NUMBER(POP()); \
        type a = AS_NUMBER(PEEK(0)); \
        vm->stack_top[-1] = value_type(func(a, b)); \
    } \
    while(false)

#define RUNTIME_ERROR(...) \
    do \
    { \
        STORE_FRAME; \
        tea_runtime_error(vm, __VA_ARGS__); \
        vm->error = false; \
        return INTERPRET_RUNTIME_ERROR; \
    } \
    while(false)

#ifdef DEBUG_TRACE_EXECUTION
    #define DEBUG() \
        do \
        { \
            printf("          "); \
            for(TeaValue* slot = vm->stack; slot < vm->stack_top; slot++) \
            { \
                printf("[ "); \
                tea_print_value(*slot); \
                printf(" ]"); \
            } \
            printf("\n"); \
            tea_disassemble_instruction(&frame->closure->function->chunk, (int)(ip - frame->closure->function->chunk.code)); \
        } \
        while(false)
#else
    #define DEBUG() do { } while (false)
#endif

#ifdef COMPUTED_GOTO

    static void* dispatch_table[] = {
        #define OPCODE(name) &&OP_##name
        #include "tea_opcodes.h"
        #undef OPCODE
    };

    #define DISPATCH() \
        do \
        { \
            DEBUG(); \
            goto *dispatch_table[instruction = READ_BYTE()]; \
        } \
        while(false)

    #define INTREPRET_LOOP  DISPATCH();
    #define CASE_CODE(name) OP_##name

#else

    #define INTREPRET_LOOP \
        loop: \
            DEBUG(); \
            switch(instruction = READ_BYTE())

    #define DISPATCH() goto loop

    #define CASE_CODE(name) case OP_##name

#endif
    uint8_t instruction;

    INTREPRET_LOOP
    {
        CASE_CODE(CONSTANT):
        {
            PUSH(READ_CONSTANT());
            DISPATCH();
        }
        CASE_CODE(NULL):
        {
            PUSH(NULL_VAL);
            DISPATCH();
        }
        CASE_CODE(TRUE):
        {
            PUSH(TRUE_VAL);
            DISPATCH();
        }
        CASE_CODE(FALSE):
        {
            PUSH(FALSE_VAL);
            DISPATCH();
        }
        CASE_CODE(DUP):
        {
            PUSH(PEEK(0));
            DISPATCH();
        }
        CASE_CODE(POP):
        {
            POP();
            DISPATCH();
        }
        CASE_CODE(POP_REPL):
        {
            TeaValue value = PEEK(0);
            if(!IS_EMPTY(value))
            {
                tea_table_set(state, &vm->globals, tea_copy_string(state, "_", 1), value);
                tea_print_value(value);
                printf("\n");
            }
            POP();
            DISPATCH();
        }
        CASE_CODE(GET_LOCAL):
        {
            uint8_t slot = READ_BYTE();
            PUSH(frame->slots[slot]);
            DISPATCH();
        }
        CASE_CODE(SET_LOCAL):
        {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = PEEK(0);
            DISPATCH();
        }
        CASE_CODE(GET_GLOBAL):
        {
            TeaObjectString* name = READ_STRING();
            TeaValue value;
            if(!tea_table_get(&vm->globals, name, &value))
            {
                RUNTIME_ERROR("Undefined variable '%s'", name->chars);
            }
            PUSH(value);
            DISPATCH();
        }
        CASE_CODE(SET_GLOBAL):
        {
            TeaObjectString* name = READ_STRING();
            if(tea_table_set(state, &vm->globals, name, PEEK(0)))
            {
                tea_table_delete(&vm->globals, name);
                RUNTIME_ERROR("Undefined variable '%s'", name->chars);
            }
            DISPATCH();
        }
        CASE_CODE(GET_MODULE):
        {
            TeaObjectString* name = READ_STRING();
            TeaValue value;
            if(!tea_table_get(&frame->closure->function->module->values, name, &value))
            {
                RUNTIME_ERROR("Undefined variable '%s'", name->chars);
            }
            PUSH(value);
            DISPATCH();
        }
        CASE_CODE(SET_MODULE):
        {
            TeaObjectString* name = READ_STRING();
            if(tea_table_set(state, &frame->closure->function->module->values, name, PEEK(0)))
            {
                tea_table_delete(&frame->closure->function->module->values, name);
                RUNTIME_ERROR("Undefined variable '%s'", name->chars);
            }
            DISPATCH();
        }
        CASE_CODE(DEFINE_GLOBAL):
        {
            TeaObjectString* name = READ_STRING();
            tea_table_set(state, &vm->globals, name, PEEK(0));
            POP();
            DISPATCH();
        }
        CASE_CODE(DEFINE_MODULE):
        {
            TeaObjectString* name = READ_STRING();
            tea_table_set(state, &frame->closure->function->module->values, name, PEEK(0));
            POP();
            DISPATCH();
        }
        CASE_CODE(GET_UPVALUE):
        {
            uint8_t slot = READ_BYTE();
            PUSH(*frame->closure->upvalues[slot]->location);
            DISPATCH();
        }
        CASE_CODE(SET_UPVALUE):
        {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = PEEK(0);
            DISPATCH();
        }
        CASE_CODE(GET_PROPERTY):
        {
            TeaValue receiver = PEEK(0);
            TeaObjectString* name = READ_STRING();
            STORE_FRAME;
            if(!get_property(vm, receiver, name, true))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(GET_PROPERTY_NO_POP):
        {
            TeaValue receiver = PEEK(0);
            TeaObjectString* name = READ_STRING();
            STORE_FRAME;
            if(!get_property(vm, receiver, name, false))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(SET_PROPERTY):
        {
            TeaObjectString* name = READ_STRING();
            TeaValue receiver = PEEK(1);
            STORE_FRAME;
            if(!set_property(vm, name, receiver))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(GET_SUPER):
        {
            TeaObjectString* name = READ_STRING();
            TeaObjectClass* superclass = AS_CLASS(POP());

            if(!bind_method(vm, superclass, name))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(RANGE):
        {
            TeaValue b = POP();
            TeaValue c = POP();
            TeaValue a = POP();

            if(!IS_NUMBER(a) || !IS_NUMBER(b)) 
            {
                RUNTIME_ERROR("Range operands must be numbers");
            }

            bool inclusive = AS_BOOL(c);

            PUSH(OBJECT_VAL(tea_new_range(state, AS_NUMBER(a), AS_NUMBER(b), inclusive)));

            DISPATCH();
        }
        CASE_CODE(LIST):
        {
            // Stack before: [item1, item2, ..., itemN] and after: [list]
            uint8_t item_count = READ_BYTE();
            if(item_count == 1 && IS_RANGE(PEEK(0)))
            {
                TeaObjectRange* range = AS_RANGE(POP());
                TeaObjectList* list_range = tea_new_list(state);
                PUSH(OBJECT_VAL(list_range));

                int from = (int)range->from;
                int to = (int)range->to;
                int count = range->inclusive ? to + 1 : to;

                for(int i = from; i < count; i++)
                {
                    tea_write_value_array(state, &list_range->items, NUMBER_VAL(i));
                }

                POP();
                PUSH(OBJECT_VAL(list_range));
                DISPATCH();
            }

            TeaObjectList* list = tea_new_list(state);

            PUSH(OBJECT_VAL(list)); // So list isn't sweeped by GC when appending the list
            // Add items to list
            for(int i = item_count; i > 0; i--)
            {
                tea_write_value_array(state, &list->items, PEEK(i));
            }
            
            // Pop items from stack
            vm->stack_top -= item_count + 1;

            PUSH(OBJECT_VAL(list));
            DISPATCH();
        }
        CASE_CODE(MAP):
        {
            uint8_t item_count = READ_BYTE();
            TeaObjectMap* map = tea_new_map(state);

            PUSH(OBJECT_VAL(map));

            for(int i = item_count * 2; i > 0; i -= 2)
            {
                if(!tea_is_valid_key(PEEK(i)))
                {
                    RUNTIME_ERROR("Map key isn't hashable");
                }

                tea_map_set(state, map, PEEK(i), PEEK(i - 1));
            }

            vm->stack_top -= item_count * 2 + 1;

            PUSH(OBJECT_VAL(map));
            DISPATCH();
        }
        CASE_CODE(SUBSCRIPT):
        {
            // Stack before: [list, index] and after: [index(list, index)]
            TeaValue index = PEEK(0);
            TeaValue list = PEEK(1);
            STORE_FRAME;
            if(!subscript(vm, index, list))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(SUBSCRIPT_STORE):
        {
            // Stack before list: [list, index, item] and after: [item]
            TeaValue item = PEEK(0);
            TeaValue index = PEEK(1);
            TeaValue list = PEEK(2);
            STORE_FRAME;
            if(!subscript_store(vm, item, index, list, true))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(SUBSCRIPT_PUSH):
        {
            // Stack before list: [list, index, item] and after: [item]
            TeaValue item = PEEK(0);
            TeaValue index = PEEK(1);
            TeaValue list = PEEK(2);
            STORE_FRAME;
            if(!subscript_store(vm, item, index, list, false))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(SLICE):
        {
            TeaValue end_index = PEEK(0);
            TeaValue start_index = PEEK(1);
            TeaValue object = PEEK(2);
            STORE_FRAME;
            if(!slice(vm, object, start_index, end_index))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(IS):
        {
            TeaValue instance = PEEK(1);
			TeaValue klass = PEEK(0);

            if(!IS_INSTANCE(instance) || !IS_CLASS(klass))
            {
				RUNTIME_ERROR("Operands must be an instance and a class");
			}

            TeaObjectClass* instance_klass = AS_INSTANCE(instance)->klass;
            TeaObjectClass* type = AS_CLASS(klass);
			bool found = false;

			while(instance_klass != NULL)
            {
				if(instance_klass == type)
                {
					found = true;
					break;
				}

				instance_klass = instance_klass->super;
			}

			DROP_MULTIPLE(2); // Drop the instance and class
			PUSH(BOOL_VAL(found));

            DISPATCH();
        }
        CASE_CODE(IN):
        {
            TeaValue object = POP();
            TeaValue value = POP();
            STORE_FRAME;
            if(!in_(vm, object, value))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            DISPATCH();
        }
        CASE_CODE(EQUAL):
        {
            TeaValue b = POP();
            TeaValue a = POP();
            PUSH(BOOL_VAL(tea_values_equal(a, b)));
            DISPATCH();
        }
        CASE_CODE(GREATER):
        {
            if(IS_STRING(PEEK(0)) && IS_STRING(PEEK(1)))
            {
                PUSH(BOOL_VAL(strcmp(AS_STRING(POP())->chars, AS_STRING(POP())->chars) > 0));
            }
            else if(IS_LIST(PEEK(0)) && IS_LIST(PEEK(1)))
            {
                PUSH(BOOL_VAL(AS_LIST(POP())->items.count < AS_LIST(POP())->items.count));
            }
            else
            {
                BINARY_OP(BOOL_VAL, >, double);
            }
            DISPATCH();
        }
        CASE_CODE(GREATER_EQUAL):
        {
            BINARY_OP(BOOL_VAL, >=, double);
            DISPATCH();
        }
        CASE_CODE(LESS):
        {
            BINARY_OP(BOOL_VAL, <, double);
            DISPATCH();
        }
        CASE_CODE(LESS_EQUAL):
        {
            BINARY_OP(BOOL_VAL, <=, double);
            DISPATCH();
        }
        CASE_CODE(ADD):
        {
            if(IS_STRING(PEEK(0)) && IS_STRING(PEEK(1)))
            {
                concatenate(vm);
            }
            else if(IS_NUMBER(PEEK(0)) && IS_NUMBER(PEEK(1)))
            {
                double b = AS_NUMBER(POP());
                double a = AS_NUMBER(POP());
                PUSH(NUMBER_VAL(a + b));
            }
            else if(IS_LIST(PEEK(0)) && IS_LIST(PEEK(1)))
            {
                TeaObjectList* l2 = AS_LIST(PEEK(0));
                TeaObjectList* l1 = AS_LIST(PEEK(1));

                TeaObjectList* l = tea_new_list(state);
                PUSH(OBJECT_VAL(l));

                for(int i = 0; i < l1->items.count; i++)
                {
                    tea_write_value_array(state, &l->items, l1->items.values[i]);
                }

                for(int i = 0; i < l2->items.count; i++)
                {
                    tea_write_value_array(state, &l->items, l2->items.values[i]);
                }

                POP();
                POP();
                POP();

                PUSH(OBJECT_VAL(l));
            }
            else if(IS_MAP(PEEK(0)) && IS_MAP(PEEK(1)))
            {
                TeaObjectMap* m2 = AS_MAP(PEEK(0));
                TeaObjectMap* m1 = AS_MAP(PEEK(1));

                tea_map_add_all(state, m2, m1);

                POP();
                POP();

                PUSH(OBJECT_VAL(m1));
            }
            else
            {
                RUNTIME_ERROR("Operands must be two numbers or two strings");
            }
            DISPATCH();
        }
        CASE_CODE(SUBTRACT):
        {
            BINARY_OP(NUMBER_VAL, -, double);
            DISPATCH();
        }
        CASE_CODE(MULTIPLY):
        {
            BINARY_OP(NUMBER_VAL, *, double);
            DISPATCH();
        }
        CASE_CODE(DIVIDE):
        {
            BINARY_OP(NUMBER_VAL, /, double);
            DISPATCH();
        }
        CASE_CODE(MOD):
        {
            BINARY_OP_FUNCTION(NUMBER_VAL, fmod, double);
            DISPATCH();
        }
        CASE_CODE(POW):
        {
            BINARY_OP_FUNCTION(NUMBER_VAL, powf, double);
            DISPATCH();
        }
        CASE_CODE(BAND):
        {
            BINARY_OP(NUMBER_VAL, &, int);
            DISPATCH();
        }
        CASE_CODE(BOR):
        {
            BINARY_OP(NUMBER_VAL, |, int);
            DISPATCH();
        }
        CASE_CODE(BNOT):
        {
            if(!IS_NUMBER(PEEK(0)))
            {
                RUNTIME_ERROR("Operand must be a number");
            }
            PUSH(NUMBER_VAL(~((int)AS_NUMBER(POP()))));

            DISPATCH();
        }
        CASE_CODE(BXOR):
        {
            BINARY_OP(NUMBER_VAL, ^, int);
            DISPATCH();
        }
        CASE_CODE(LSHIFT):
        {
            BINARY_OP(NUMBER_VAL, <<, int);
            DISPATCH();
        }
        CASE_CODE(RSHIFT):
        {
            BINARY_OP(NUMBER_VAL, >>, int);
            DISPATCH();
        }
        CASE_CODE(AND):
        {
            uint16_t offset = READ_SHORT();
            
            if(tea_is_falsey(PEEK(0)))
            {
                ip += offset;
            }
            else
            {
                DROP();
            }

            DISPATCH();
        }
        CASE_CODE(OR):
        {
            uint16_t offset = READ_SHORT();
            
            if(tea_is_falsey(PEEK(0)))
            {
                DROP();
            }
            else
            {
                ip += offset;
            }

            DISPATCH();
        }
        CASE_CODE(NOT):
        {
            PUSH(BOOL_VAL(tea_is_falsey(POP())));
            DISPATCH();
        }
        CASE_CODE(NEGATE):
        {
            if(!IS_NUMBER(PEEK(0)))
            {
                RUNTIME_ERROR("Operand must be a number");
            }
            PUSH(NUMBER_VAL(-AS_NUMBER(POP())));
            DISPATCH();
        }
        CASE_CODE(MULTI_CASE):
        {
            int count = READ_BYTE();
            TeaValue switch_value = PEEK(count + 1);
            TeaValue case_value = POP();
            for(int i = 0; i < count; i++)
            {
                if(tea_values_equal(switch_value, case_value))
                {
                    i++;
                    while(i <= count)
                    {
                        POP();
                        i++;   
                    }
                    break;
                }
                case_value = POP();
            }
            PUSH(case_value);
            DISPATCH();
        }
        CASE_CODE(COMPARE_JUMP):
        {
            uint16_t offset = READ_SHORT();
            TeaValue a = POP();
            if(!tea_values_equal(PEEK(0), a))
            {
                ip += offset;
            }
            else
            {
                POP();
            }
            DISPATCH();
        }
        CASE_CODE(JUMP):
        {
            uint16_t offset = READ_SHORT();
            ip += offset;
            DISPATCH();
        }
        CASE_CODE(JUMP_IF_FALSE):
        {
            uint16_t offset = READ_SHORT();
            if(tea_is_falsey(PEEK(0)))
            {
                ip += offset;
            }
            DISPATCH();
        }
        CASE_CODE(JUMP_IF_NULL):
        {
            uint16_t offset = READ_SHORT();
            if(IS_NULL(PEEK(0)))
            {
                ip += offset;
            }
            DISPATCH();
        }
        CASE_CODE(LOOP):
        {
            uint16_t offset = READ_SHORT();
            ip -= offset;
            DISPATCH();
        }
        CASE_CODE(CALL):
        {
            int arg_count = READ_BYTE();
            STORE_FRAME;
            if(!call_value(vm, PEEK(arg_count), arg_count))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm->frames[vm->frame_count - 1];
            ip = frame->ip;
            DISPATCH();
        }
        CASE_CODE(INVOKE):
        {
            TeaObjectString* method = READ_STRING();
            int arg_count = READ_BYTE();
            STORE_FRAME;
            if(!invoke(vm, PEEK(arg_count), method, arg_count))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm->frames[vm->frame_count - 1];
            ip = frame->ip;
            DISPATCH();
        }
        CASE_CODE(SUPER):
        {
            TeaObjectString* method = READ_STRING();
            int arg_count = READ_BYTE();
            STORE_FRAME;
            TeaObjectClass* superclass = AS_CLASS(POP());
            if(!invoke_from_class(vm, superclass, method, arg_count))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm->frames[vm->frame_count - 1];
            ip = frame->ip;
            DISPATCH();
        }
        CASE_CODE(CLOSURE):
        {
            TeaObjectFunction* function = AS_FUNCTION(READ_CONSTANT());
            TeaObjectClosure* closure = tea_new_closure(state, function);
            PUSH(OBJECT_VAL(closure));
            
            for(int i = 0; i < closure->upvalue_count; i++)
            {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if(isLocal)
                {
                    closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
                }
                else
                {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
            DISPATCH();
        }
        CASE_CODE(CLOSE_UPVALUE):
        {
            close_upvalues(vm, vm->stack_top - 1);
            POP();
            DISPATCH();
        }
        CASE_CODE(RETURN):
        {
            TeaValue result = POP();
            close_upvalues(vm, frame->slots);
            vm->frame_count--;
            if(vm->frame_count == 0)
            {
                POP();
                return INTERPRET_OK;
            }

            vm->stack_top = frame->slots;
            PUSH(result);
            frame = &vm->frames[vm->frame_count - 1];
            ip = frame->ip;
            DISPATCH();
        }
        CASE_CODE(CLASS):
        {
            PUSH(OBJECT_VAL(tea_new_class(state, READ_STRING(), NULL)));
            DISPATCH();
        }
        CASE_CODE(SET_CLASS_VAR):
        {
            TeaObjectClass* klass = AS_CLASS(PEEK(1));
            TeaObjectString* key = READ_STRING();

            tea_table_set(state, &klass->statics, key, PEEK(0));
            POP();
            DISPATCH();
        }
        CASE_CODE(INHERIT):
        {
            TeaValue super = PEEK(1);

            if(!IS_CLASS(super))
            {
                RUNTIME_ERROR("Superclass must be a class");
            }

            TeaObjectClass* superclass = AS_CLASS(super);
            TeaObjectClass* klass = AS_CLASS(POP());
            klass->super = superclass;
            
            tea_table_add_all(state, &superclass->methods, &klass->methods);
            tea_table_add_all(state, &superclass->statics, &klass->statics);
            POP();   // Subclass
            DISPATCH();
        }
        CASE_CODE(METHOD):
        {
            define_method(vm, READ_STRING());
            DISPATCH();
        }
        CASE_CODE(IMPORT):
        {
            TeaObjectString* file_name = READ_STRING();
            TeaValue module_value;

            // If we have imported this file already, skip.
            if(tea_table_get(&vm->modules, file_name, &module_value)) 
            {
                vm->last_module = AS_MODULE(module_value);
                PUSH(NULL_VAL);
                DISPATCH();
            }

            char path[PATH_MAX];
            if(!tea_resolve_path(frame->closure->function->module->path->chars, file_name->chars, path))
            {
                RUNTIME_ERROR("Could not open file \"%s\"", file_name->chars);
            }

            char* source = tea_read_file(state, path);

            if(source == NULL) 
            {
                RUNTIME_ERROR("Could not open file \"%s\"", file_name->chars);
            }

            TeaObjectString* path_obj = tea_copy_string(state, path, strlen(path));
            TeaObjectModule* module = tea_new_module(state, path_obj);
            module->path = tea_dirname(state, path, strlen(path));
            vm->last_module = module;

            TeaObjectFunction* function = tea_compile(state, module, source);

            FREE_ARRAY(state, char, source, strlen(source) + 1);

            if(function == NULL) return INTERPRET_COMPILE_ERROR;
            TeaObjectClosure* closure = tea_new_closure(state, function);
            PUSH(OBJECT_VAL(closure));

            frame->ip = ip;
            call(vm, closure, 0);
            frame = &vm->frames[vm->frame_count - 1];
            ip = frame->ip;

            DISPATCH();
        }
        CASE_CODE(IMPORT_VARIABLE):
        {
            PUSH(OBJECT_VAL(vm->last_module));
            DISPATCH();
        }
        CASE_CODE(IMPORT_FROM):
        {
            int var_count = READ_BYTE();

            for(int i = 0; i < var_count; i++) 
            {
                TeaValue module_variable;
                TeaObjectString* variable = READ_STRING();

                if(!tea_table_get(&vm->last_module->values, variable, &module_variable)) 
                {
                    RUNTIME_ERROR("%s can't be found in module %s", variable->chars, vm->last_module->name->chars);
                }

                PUSH(module_variable);
            }

            DISPATCH();
        }
        CASE_CODE(IMPORT_END):
        {
            vm->last_module = frame->closure->function->module;
            DISPATCH();
        }
        CASE_CODE(IMPORT_NATIVE):
        {
            int index = READ_BYTE();
            TeaObjectString* file_name = READ_STRING();

            TeaValue module_val;
            // If the module is already imported, skip
            if(tea_table_get(&vm->modules, file_name, &module_val))
            {
                vm->last_module = AS_MODULE(module_val);
                PUSH(module_val);
                DISPATCH();
            }

            TeaValue module = tea_import_native_module(vm, index);

            PUSH(module);

            if(IS_CLOSURE(module)) 
            {
                STORE_FRAME;
                call(vm, AS_CLOSURE(module), 0);
                frame = &vm->frames[vm->frame_count - 1];
                ip = frame->ip;

                tea_table_get(&vm->modules, file_name, &module);
                vm->last_module = AS_MODULE(module);
            }

            DISPATCH();
        }
        CASE_CODE(IMPORT_NATIVE_VARIABLE):
        {
            TeaObjectString* file_name = READ_STRING();
            int var_count = READ_BYTE();

            TeaObjectModule* module;

            TeaValue module_val;
            if(tea_table_get(&vm->modules, file_name, &module_val)) 
            {
                module = AS_MODULE(module_val);
            } 

            for(int i = 0; i < var_count; i++) 
            {
                TeaObjectString* variable = READ_STRING();

                TeaValue module_variable;
                if(!tea_table_get(&module->values, variable, &module_variable)) 
                {
                    RUNTIME_ERROR("%s can't be found in module %s", variable->chars, module->name->chars);
                }

                PUSH(module_variable);
            }

            DISPATCH();
        }
        CASE_CODE(END):
        {
            DISPATCH();
        }
    }

    return INTERPRET_RUNTIME_ERROR;

#undef PUSH
#undef POP
#undef PEEK
#undef DROP
#undef DROP_MULTIPLE
#undef STORE_FRAME
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
#undef BINARY_OP_FUNCTION
#undef RUNTIME_ERROR
}

TeaInterpretResult tea_interpret_module(TeaState* state, const char* module_name, const char* source)
{
    register TeaVM* vm = state->vm;

    TeaObjectString* name = tea_copy_string(state, module_name, strlen(module_name));
    TeaObjectModule* module = tea_new_module(state, name);

    module->path = tea_get_directory(state, (char*)module_name);

    TeaObjectFunction* function = tea_compile(state, module, source);
    if(function == NULL)
        return INTERPRET_COMPILE_ERROR;

    TeaObjectClosure* closure = tea_new_closure(state, function);
    tea_push(vm, OBJECT_VAL(closure));
    call(vm, closure, 0);

    return run_interpreter(state);
}