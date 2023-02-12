// tea_vm.c
// Teascript virtual machine

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
#include "tea_module.h"

static inline void push(TeaState* T, TeaValue value)
{
    *T->thread->stack_top++ = value;
}

static inline TeaValue pop(TeaState* T)
{
    return *(--T->thread->stack_top);
}

static inline TeaValue peek(TeaState* T, int distance)
{
    return T->thread->stack_top[-1 - (distance)];
}

static void reset_stack(TeaState* T)
{
    if(T->thread != NULL)
    {
        T->thread->stack_top = T->thread->stack;
    }
}

void tea_runtime_error(TeaState* T, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for(int i = T->thread->frame_count - 1; i >= 0; i--)
    {
        TeaCallFrame* frame = &T->thread->frames[i];
        TeaObjectFunction* function = frame->closure->function;
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);
        if(function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    reset_stack(T);
}

static bool in_(TeaState* T, TeaValue object, TeaValue value)
{
    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_STRING:
            {
                if(!IS_STRING(value))
                {
                    pop(T);
                    pop(T);
                    push(T, FALSE_VAL);
                    return true;
                }

                TeaObjectString* string = AS_STRING(object);
                TeaObjectString* sub = AS_STRING(value);

                if(sub == string)
                {
                    pop(T);
                    pop(T);
                    push(T, TRUE_VAL);
                    return true;
                }

                pop(T);
                pop(T);
                push(T, BOOL_VAL(strstr(string->chars, sub->chars) != NULL));
                return true;
            }
            case OBJ_RANGE:
            {
                if(!IS_NUMBER(value))
                {
                    push(T, FALSE_VAL);
                    return true;
                }

                double number = AS_NUMBER(value);
                TeaObjectRange* range = AS_RANGE(object);
                int start = range->start;
                int end = range->end;

                if(number < start || number > end)
                {
                    pop(T);
                    pop(T);
                    push(T, FALSE_VAL);
                    return true;
                }

                pop(T);
                pop(T);
                push(T, TRUE_VAL);
                return true;
            }
            case OBJ_LIST:
            {
                TeaObjectList* list = AS_LIST(object);

                for(int i = 0; i < list->items.count; i++) 
                {
                    if(tea_values_equal(list->items.values[i], value)) 
                    {
                        pop(T);
                        pop(T);
                        push(T, TRUE_VAL);
                        return true;
                    }
                }

                pop(T);
                pop(T);
                push(T, FALSE_VAL);
                return true;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(object);
                TeaValue _;

                pop(T);
                pop(T);
                push(T, BOOL_VAL(tea_map_get(map, value, &_)));
                return true;
            }
            default:
                break;
        }
    }

    tea_runtime_error(T, "%s is not an iterable", tea_value_type(object));
    return false;
}

static bool slice(TeaState* T, TeaValue object, TeaValue start_index, TeaValue end_index, TeaValue step_index)
{
    if(!IS_NUMBER(step_index) || (!IS_NUMBER(end_index) && !IS_NULL(end_index)) || !IS_NUMBER(step_index)) 
    {
        tea_runtime_error(T, "Slice index must be a number");
        return false;
    }

    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_LIST:
            {
                TeaObjectList* new_list = tea_new_list(T);
                push(T, OBJECT_VAL(new_list));
                TeaObjectList* list = AS_LIST(object);

                int start = AS_NUMBER(start_index);
                int end;
                int step = AS_NUMBER(step_index);

                if(IS_NULL(end_index))
                {
                    end = list->items.count;
                }
                else
                {
                    end = AS_NUMBER(end_index);
                    if(end > list->items.count)
                    {
                        end = list->items.count;
                    }
                    else if(end < 0)
                    {
                        end = list->items.count + end;
                    }
                }

                if(step > 0)
                {
                    for(int i = start; i < end; i += step)
                    {
                        tea_write_value_array(T, &new_list->items, list->items.values[i]);
                    }
                }
                else if(step < 0)
                {
                    for(int i = end + step; i >= start; i += step)
                    {
                        tea_write_value_array(T, &new_list->items, list->items.values[i]);
                    }
                }

                pop(T);
                
                pop(T);
                pop(T);
                pop(T);
                pop(T);
                push(T, OBJECT_VAL(new_list));
                return true;
            }
            case OBJ_STRING:
            {
                TeaObjectString* string = AS_STRING(object);
                int length = tea_ustring_length(string);

                int start = AS_NUMBER(start_index);
                int end;

                if(IS_NULL(end_index))
                {
                    end = string->length;
                }
                else
                {
                    end = AS_NUMBER(end_index);
                    if(end > length) 
                    {
                        end = length;
                    }
                    else if(end < 0) 
                    {
                        end = length + end;
                    }
                }

                // Ensure the start index is below the end index
                if(start > end)
                {
                    pop(T);
                    pop(T);
                    pop(T);
                    pop(T);
                    push(T, OBJECT_VAL(tea_copy_string(T, "", 0)));
                    return true;
                } 
                else 
                {
                    start = tea_uchar_offset(string->chars, start);
                    end = tea_uchar_offset(string->chars, end);
                    pop(T);
                    pop(T);
                    pop(T);
                    pop(T);
                    push(T, OBJECT_VAL(tea_ustring_from_range(T, string, start, end - start)));
                    return true;
                }
            }
        }
    }

    tea_runtime_error(T, "%s is not slicable", tea_value_type(object));
    return false;
}

static bool subscript(TeaState* T, TeaValue index_value, TeaValue subscript_value)
{
    if(IS_OBJECT(subscript_value))
    {
        switch(OBJECT_TYPE(subscript_value))
        {
            case OBJ_RANGE:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error(T, "Range index must be a number");
                    return false;
                }

                TeaObjectRange* range = AS_RANGE(subscript_value);
                double index = AS_NUMBER(index_value);

                // Calculate the length of the range
                double len = (range->end - range->start) / range->step;

                // Allow negative indexes
                if(index < 0)
                {
                    index = len + index;
                }

                if(index >= 0 && index < len) 
                {
                    pop(T);
                    pop(T);
                    push(T, NUMBER_VAL(range->start + index * range->step));
                    return true;
                }

                tea_runtime_error(T, "Range index out of bounds");
                return false;
            }
            case OBJ_LIST:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error(T, "List index must be a number");
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
                    pop(T);
                    pop(T);
                    push(T, list->items.values[index]);
                    return true;
                }

                tea_runtime_error(T, "List index out of bounds");
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(subscript_value);
                if(!tea_is_valid_key(index_value))
                {
                    tea_runtime_error(T, "Map key isn't hashable");
                    return false;
                }

                TeaValue value;
                pop(T);
                pop(T);
                if(tea_map_get(map, index_value, &value))
                {
                    push(T, value);
                    return true;
                }

                tea_runtime_error(T, "Key does not exist within map");
                return false;
            }
            case OBJ_STRING:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error(T, "String index must be a number (got %s)", tea_value_type(index_value));
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
                    pop(T);
                    pop(T);
                    TeaObjectString* c = tea_ustring_code_point_at(T, string, tea_uchar_offset(string->chars, index));
                    push(T, OBJECT_VAL(c));
                    return true;
                }

                tea_runtime_error(T, "String index out of bounds");
                return false;
            }
            default:
                break;
        }
    }
    
    tea_runtime_error(T, "%s is not subscriptable", tea_value_type(subscript_value));
    return false;
}

static bool subscript_store(TeaState* T, TeaValue item_value, TeaValue index_value, TeaValue subscript_value, bool assign)
{
    if(IS_OBJECT(subscript_value))
    {
        switch(OBJECT_TYPE(subscript_value))
        {
            case OBJ_LIST:
            {
                if(!IS_NUMBER(index_value)) 
                {
                    tea_runtime_error(T, "List index must be a number (got %s)", tea_value_type(index_value));
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
                        pop(T);
                        pop(T);
                        pop(T);
                        push(T, item_value);
                    }
                    else
                    {
                        T->thread->stack_top[-1] = list->items.values[index];
                        push(T, item_value);
                    }
                    return true;
                }

                tea_runtime_error(T, "List index out of bounds");
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(subscript_value);
                if(!tea_is_valid_key(index_value))
                {
                    tea_runtime_error(T, "Map key isn't hashable");
                    return false;
                }

                if(assign)
                {
                    tea_map_set(T, map, index_value, item_value);
                    pop(T);
                    pop(T);
                    pop(T);
                    push(T, item_value);
                }
                else
                {
                    TeaValue map_value;
                    if(!tea_map_get(map, index_value, &map_value))
                    {
                        tea_runtime_error(T, "Key does not exist within the map");
                        return false;
                    }
                    T->thread->stack_top[-1] = map_value;
                    push(T, item_value);
                }
                
                return true;
            }
            default:
                break;
        }
    }

    tea_runtime_error(T, "%s does not support item assignment", tea_value_type(subscript_value));
    return false;
}

static bool call(TeaState* T, TeaObjectClosure* closure, int arg_count)
{
    if(arg_count < closure->function->arity)
    {
        if((arg_count + closure->function->variadic) == closure->function->arity)
        {
            // add missing variadic param ([])
            TeaObjectList* list = tea_new_list(T);
            push(T, OBJECT_VAL(list));
            arg_count++;
        }
        else
        {
            tea_runtime_error(T, "Expected %d arguments, but got %d", closure->function->arity, arg_count);
            return false;
        }
    }
    else if(arg_count > closure->function->arity + closure->function->arity_optional)
    {
        if(closure->function->variadic)
        {
            int arity = closure->function->arity + closure->function->arity_optional;
            // +1 for the variadic param itself
            int varargs = arg_count - arity + 1;
            TeaObjectList* list = tea_new_list(T);
            push(T, OBJECT_VAL(list));
            for(int i = varargs; i > 0; i--)
            {
                tea_write_value_array(T, &list->items, peek(T, i));
            }
            // +1 for the list pushed earlier on the stack
            T->thread->stack_top -= varargs + 1;
            push(T, OBJECT_VAL(list));
            arg_count = arity;
        }
        else
        {
            tea_runtime_error(T, "Expected %d arguments, but got %d", closure->function->arity + closure->function->arity_optional, arg_count);
            return false;
        }
    }
    else if(closure->function->variadic)
    {
        // last argument is the variadic arg
        TeaObjectList* list = tea_new_list(T);
        push(T, OBJECT_VAL(list));
        tea_write_value_array(T, &list->items, peek(T, 1));
        T->thread->stack_top -= 2;
        push(T, OBJECT_VAL(list));
    }

    if(T->thread->frame_count == 1000)
    {
        tea_runtime_error(T, "Stack overflow");
        return false;
    }

    tea_ensure_callframe(T, T->thread);

    int stack_size = (int)(T->thread->stack_top - T->thread->stack);
    int needed = stack_size + closure->function->max_slots;
	tea_ensure_stack(T, T->thread, needed);

    tea_append_callframe(T, T->thread, closure, T->thread->stack_top - arg_count - 1);

    return true;
}

static bool call_native_property(TeaState* T, TeaObjectNative* native)
{
    // a fake caller
    T->slot[T->top++] = NULL_VAL;

    // move function arguments to c stack
    T->slot[T->top++] = *(T->thread->stack_top - 1);

    TeaStackInfo* info = &T->infos[T->info_count++];
    info->slot = T->slot; // Save the start of last slot
    info->top = T->top - 2;    // Save top of the last slot

    T->slot = T->slot - 1 + T->top;
    T->top = 1;

    native->fn(T);

    TeaValue ret = T->slot[T->top - 1];
    info = &T->infos[--T->info_count];
    T->slot = info->slot;     // Offset slot back to last origin
    T->top = info->top;    // Get last top of origin slot

    T->thread->stack_top -= 1;
    push(T, ret);
    return true;
}

static bool call_native(TeaState* T, TeaObjectNative* native, uint8_t arg_count)
{
    // a fake caller
    T->slot[T->top++] = NULL_VAL;

    int n = 0;
    if(native->type == NATIVE_METHOD)
    {
        T->slot[T->top++] = T->thread->stack_top[-arg_count - 1];
        n = 1;
    }

    // move function arguments to c stack
    for(TeaValue* slot = T->thread->stack_top - arg_count; slot <= T->thread->stack_top-1; slot++)
    {
        T->slot[T->top++] = *slot;
    }

    TeaStackInfo* info = &T->infos[T->info_count++];
    info->slot = T->slot; // Save the start of last slot
    info->top = T->top - (n + arg_count) - 1;    // Save top of the last slot

    T->slot = T->slot - (n + arg_count) + T->top;
    T->top = arg_count + n;

    native->fn(T);

    TeaValue ret = T->slot[T->top - 1];
    info = &T->infos[--T->info_count];
    T->slot = info->slot;     // Offset slot back to last origin
    T->top = info->top;    // Get last top of origin slot

    T->thread->stack_top -= arg_count + 1;
    push(T, ret);
    return true;
}

static bool call_value(TeaState* T, TeaValue callee, uint8_t arg_count)
{
    if(IS_OBJECT(callee))
    {
        switch(OBJECT_TYPE(callee))
        {
            case OBJ_BOUND_METHOD:
            {
                TeaObjectBoundMethod* bound = AS_BOUND_METHOD(callee);
                T->thread->stack_top[-arg_count - 1] = bound->receiver;
                return call_value(T, bound->method, arg_count);
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(callee);
                T->thread->stack_top[-arg_count - 1] = OBJECT_VAL(tea_new_instance(T, klass));
                if(!IS_NULL(klass->constructor)) 
                {
                    return call_value(T, klass->constructor, arg_count);
                }
                else if(arg_count != 0)
                {
                    tea_runtime_error(T, "Expected 0 arguments but got %d", arg_count);
                    return false;
                }
                return true;
            }
            case OBJ_CLOSURE:
                return call(T, AS_CLOSURE(callee), arg_count);
            case OBJ_NATIVE:
            {
                TeaObjectNative* native = AS_NATIVE(callee);
                if(native->type == NATIVE_PROPERTY) break;

                if(tea_set_jump(T))
                {
                    return false;
                }

                return call_native(T, native, arg_count);
            }
            default:
                break; // Non-callable object type
        }
    }

    tea_runtime_error(T, "%s is not callable", tea_value_type(callee));
    return false;
}

static bool invoke_from_class(TeaState* T, TeaObjectClass* klass, TeaObjectString* name, int arg_count)
{
    TeaValue method;
    if(!tea_table_get(&klass->methods, name, &method))
    {
        tea_runtime_error(T, "Undefined property '%s'", name->chars);
        return false;
    }

    return call(T, AS_CLOSURE(method), arg_count);
}

static bool invoke(TeaState* T, TeaValue receiver, TeaObjectString* name, int arg_count)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(receiver);

                TeaValue value;
                if(!tea_table_get(&module->values, name, &value)) 
                {
                    tea_runtime_error(T, "Undefined property '%s' in '%s' module", name->chars, module->name->chars);
                    return false;
                }

                return call_value(T, value, arg_count);
            }
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);

                TeaValue value;
                if(tea_table_get(&instance->fields, name, &value))
                {
                    T->thread->stack_top[-arg_count - 1] = value;
                    return call_value(T, value, arg_count);
                }

                if(tea_table_get(&instance->klass->methods, name, &value)) 
                {
                    return call_value(T, value, arg_count);
                }

                tea_runtime_error(T, "Undefined property '%s'", name->chars);
                return false;
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(receiver);
                TeaValue method;
                if(tea_table_get(&klass->methods, name, &method)) 
                {
                    if(IS_NATIVE(method) || AS_CLOSURE(method)->function->type != TYPE_STATIC) 
                    {
                        tea_runtime_error(T, "'%s' is not static. Only static methods can be invoked directly from a class", name->chars);
                        return false;
                    }

                    return call_value(T, method, arg_count);
                }

                tea_runtime_error(T, "Undefined property '%s'", name->chars);
                return false;
            }
            default:
            {
                TeaObjectClass* type = tea_get_class(T, receiver);
                if(type != NULL)
                {
                    TeaValue value;
                    if(tea_table_get(&type->methods, name, &value)) 
                    {
                        return call_value(T, value, arg_count);
                    }

                    tea_runtime_error(T, "%s has no method %s()", tea_object_type(receiver), name->chars);
                    return false;
                }
            }
        }
    }

    tea_runtime_error(T, "Only objects have methods, %s given", tea_value_type(receiver));
    return false;
}

static bool bind_method(TeaState* T, TeaObjectClass* klass, TeaObjectString* name)
{
    TeaValue method;
    if(!tea_table_get(&klass->methods, name, &method))
    {
        tea_runtime_error(T, "Undefined property '%s'", name->chars);
        return false;
    }

    TeaObjectBoundMethod* bound = tea_new_bound_method(T, peek(T, 0), method);
    pop(T);
    push(T, OBJECT_VAL(bound));
    return true;
}

static bool get_property(TeaState* T, TeaValue receiver, TeaObjectString* name, bool dopop)
{
    if(IS_OBJECT(receiver))
    {   
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);
                
                TeaValue value;
                if(tea_table_get(&instance->fields, name, &value))
                {
                    if(dopop)
                    {
                        pop(T); // Instance.
                    }
                    push(T, value);
                    return true;
                }

                if(!bind_method(T, instance->klass, name))
                {
                    return false;
                }

                TeaObjectClass* klass = instance->klass;
                while(klass != NULL) 
                {
                    if(tea_table_get(&klass->statics, name, &value))
                    {
                        if(dopop)
                        {
                            pop(T); // Instance
                        }
                        push(T, value);
                        return true;
                    }

                    klass = klass->super;
                }

                tea_runtime_error(T, "'%s' instance has no property: '%s'", instance->klass->name->chars, name->chars);
                return false;
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(receiver);
                TeaObjectClass* klass_store = klass;

                while(klass != NULL) 
                {
                    TeaValue value;
                    if(tea_table_get(&klass->statics, name, &value) || tea_table_get(&klass->methods, name, &value))
                    {
                        if(dopop)
                        {
                            pop(T); // Class
                        }
                        push(T, value);
                        return true;
                    }

                    klass = klass->super;
                }

                tea_runtime_error(T, "'%s' class has no property: '%s'.", klass_store->name->chars, name->chars);
                return false;
            }
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(receiver);

                TeaValue value;
                if(tea_table_get(&module->values, name, &value)) 
                {
                    if(dopop)
                    {
                        pop(T); // Module
                    }
                    push(T, value);
                    return true;
                }

                tea_runtime_error(T, "'%s' module has no property: '%s'", module->name->chars, name->chars);
                return false;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(receiver);

                TeaValue value;
                if(tea_map_get(map, OBJECT_VAL(name), &value))
                {
                    if(dopop)
                    {
                        pop(T);
                    }
                    push(T, value);
                    return true;
                }
                else
                {
                    goto try;
                }
                
                tea_runtime_error(T, "map has no property: '%s'", name->chars);
                return false;
            }
            default:
            {
                try:
                TeaObjectClass* type = tea_get_class(T, receiver);
                if(type != NULL)
                {
                    TeaValue value;
                    if(tea_table_get(&type->methods, name, &value)) 
                    {
                        if(IS_NATIVE_PROPERTY(value))
                        {
                            return call_native_property(T, AS_NATIVE(value));
                        }
                        pop(T);
                        push(T, value);
                        return true;
                    }
                }
            }
        }
        tea_runtime_error(T, "%s has no property '%s'", tea_object_type(receiver), name->chars);
        return false;
    }

    tea_runtime_error(T, "Only objects have properties");
    return false;
}

static bool set_property(TeaState* T, TeaObjectString* name, TeaValue receiver, TeaValue item)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_INSTANCE:
            {
                TeaObjectInstance* instance = AS_INSTANCE(receiver);
                tea_table_set(T, &instance->fields, name, item);
                pop(T);
                pop(T);
                push(T, item);
                return true;
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(receiver);
                tea_table_set(T, &klass->statics, name, item);
                pop(T);
                pop(T);
                push(T, item);
                return true;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(receiver);
                tea_map_set(T, map, OBJECT_VAL(name), item);
                pop(T);
                pop(T);
                push(T, item);
                return true;
            }
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(receiver);
                tea_table_set(T, &module->values, name, item);
                pop(T);
                pop(T);
                push(T, item);
                return true;
            }
            default:
                break;
        }
    }

    tea_runtime_error(T, "Cannot set property on type %s", tea_value_type(receiver));
    return false;
}

static TeaObjectUpvalue* capture_upvalue(TeaState* T, TeaValue* local)
{
    TeaObjectUpvalue* prev_upvalue = NULL;
    TeaObjectUpvalue* upvalue = T->thread->open_upvalues;
    while(upvalue != NULL && upvalue->location > local)
    {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    if(upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    TeaObjectUpvalue* created_upvalue = tea_new_upvalue(T, local);
    created_upvalue->next = upvalue;

    if(prev_upvalue == NULL)
    {
        T->thread->open_upvalues = created_upvalue;
    }
    else
    {
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

static void close_upvalues(TeaState* T, TeaValue* last)
{
    while(T->thread->open_upvalues != NULL && T->thread->open_upvalues->location >= last)
    {
        TeaObjectUpvalue* upvalue = T->thread->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        T->thread->open_upvalues = upvalue->next;
    }
}

static void define_method(TeaState* T, TeaObjectString* name)
{
    TeaValue method = peek(T, 0);
    TeaObjectClass* klass = AS_CLASS(peek(T, 1));
    tea_table_set(T, &klass->methods, name, method);
    if(name == T->constructor_string) klass->constructor = method;
    pop(T);
}

static void concatenate(TeaState* T)
{
    TeaObjectString* b = AS_STRING(peek(T, 0));
    TeaObjectString* a = AS_STRING(peek(T, 1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(T, char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    TeaObjectString* result = tea_take_string(T, chars, length);
    pop(T);
    pop(T);
    push(T, OBJECT_VAL(result));
}

static void repeat(TeaState* T)
{
    TeaObjectString* string;
    int n;

    if(IS_STRING(peek(T, 0)) && IS_NUMBER(peek(T, 1)))
    {
        string = AS_STRING(peek(T, 0));
        n = AS_NUMBER(peek(T, 1));
    }
    else if(IS_NUMBER(peek(T, 0)) && IS_STRING(peek(T, 1)))
    {
        n = AS_NUMBER(peek(T, 0));
        string = AS_STRING(peek(T, 1));
    }

    if(n <= 0)
    {
        TeaObjectString* s = tea_copy_string(T, "", 0);
        pop(T);
        pop(T);
        push(T, OBJECT_VAL(s));
        return;
    }
    else if(n == 1)
    {
        pop(T);
        pop(T);
        push(T, OBJECT_VAL(string));
        return;
    }

    int length = string->length;
    char* chars = ALLOCATE(T, char, (n * length) + 1);

    int i; 
    char* p;
    for(i = 0, p = chars; i < n; ++i, p += length)
    {
        memcpy(p, string->chars, length);
    }
    *p = '\0';

    TeaObjectString* result = tea_take_string(T, chars, strlen(chars));
    pop(T);
    pop(T);
    push(T, OBJECT_VAL(result));
}

static TeaInterpretResult run_interpreter(TeaState* T, register TeaObjectThread* thread)
{
    T->thread = thread;
    T->thread->type = THREAD_ROOT;

    register TeaCallFrame* frame;
    register TeaChunk* current_chunk;

    register uint8_t* ip;
    register TeaValue* slots;
    register TeaObjectUpvalue** upvalues;

#define PUSH(value) (*thread->stack_top++ = value)
#define POP() (*(--thread->stack_top))
#define PEEK(distance) thread->stack_top[-1 - (distance)]
#define DROP() (thread->stack_top--)
#define DROP_MULTIPLE(amount) (thread->stack_top -= amount)
#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

#define STORE_FRAME (frame->ip = ip)
#define READ_FRAME() \
    do \
    { \
        frame = &thread->frames[thread->frame_count - 1]; \
        current_chunk = &frame->closure->function->chunk; \
	    ip = frame->ip; \
	    slots = frame->slots; \
	    upvalues = frame->closure == NULL ? NULL : frame->closure->upvalues; \
    } \
    while(false) \

#define READ_CONSTANT() (current_chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define RUNTIME_ERROR(...) \
    do \
    { \
        STORE_FRAME; \
        tea_runtime_error(T, __VA_ARGS__); \
        thread = T->thread; \
        if(thread != NULL) \
        { \
            return TEA_RUNTIME_ERROR; \
        } \
        READ_FRAME(); \
        DISPATCH(); \
    } \
    while(false)

#define INVOKE_METHOD(a, b, name, arg_count) \
    do \
    { \
        TeaObjectString* method_name = tea_copy_string(T, name, strlen(name)); \
        TeaValue method; \
        if(((IS_INSTANCE(a) && IS_INSTANCE(b)) || IS_INSTANCE(a)) && tea_table_get(&AS_INSTANCE(a)->klass->methods, method_name, &method)) \
        { \
            STORE_FRAME; \
            if(!call_value(T, method, arg_count)) \
            { \
                return TEA_RUNTIME_ERROR; \
            } \
            READ_FRAME(); \
            DISPATCH(); \
        } \
        else if(IS_INSTANCE(b) && tea_table_get(&AS_INSTANCE(b)->klass->methods, method_name, &method)) \
        { \
            STORE_FRAME; \
            if(!call_value(T, method, arg_count)) \
            { \
                return TEA_RUNTIME_ERROR; \
            } \
            READ_FRAME(); \
            DISPATCH(); \
        } \
        else \
        { \
            RUNTIME_ERROR("Undefined '%s' overload", name); \
        } \
    } \
    while(false);

#define BINARY_OP(value_type, op, op_string, type) \
    do \
    { \
        if(IS_NUMBER(PEEK(0)) && IS_NUMBER(PEEK(1))) \
        { \
            type b = AS_NUMBER(POP()); \
            type a = AS_NUMBER(PEEK(0)); \
            thread->stack_top[-1] = value_type(a op b); \
        } \
        else if(IS_INSTANCE(PEEK(1)) || IS_INSTANCE(PEEK(0))) \
        { \
            TeaValue a = PEEK(1); \
            TeaValue b = PEEK(0); \
            DROP(); \
            PUSH(a); \
            PUSH(b); \
            INVOKE_METHOD(a, b, op_string, 2); \
        } \
        else \
        { \
            RUNTIME_ERROR("Attempt to use %s operator with %s and %s", op_string, tea_value_type(PEEK(1)), tea_value_type(PEEK(0))); \
        } \
    } \
    while(false)

#ifdef DEBUG_TRACE_EXECUTION
    #define TRACE_INSTRUCTIONS() \
        do \
        { \
            tea_disassemble_instruction(T, current_chunk, (int)(ip - current_chunk->code)); \
        } \
        while(false)
#else
    #define TRACE_INSTRUCTIONS() do { } while(false)
#endif

#ifdef COMPUTED_GOTO
    static void* dispatch_table[] = {
        #define OPCODE(name, _) &&OP_##name
        #include "tea_opcodes.h"
        #undef OPCODE
    };

    #define DISPATCH() \
        do \
        { \
            TRACE_INSTRUCTIONS(); \
            goto *dispatch_table[instruction = READ_BYTE()]; \
        } \
        while(false)

    #define INTREPRET_LOOP  DISPATCH();
    #define CASE_CODE(name) OP_##name
#else
    #define INTREPRET_LOOP \
        loop: \
            switch(instruction = READ_BYTE())

    #define DISPATCH() goto loop

    #define CASE_CODE(name) case OP_##name
#endif

    READ_FRAME();
    
    while(true)
    {
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
                if(!IS_NULL(value))
                {
                    TeaObjectString* string = tea_value_tostring(T, value);
                    PUSH(OBJECT_VAL(string));
                    tea_write_string(string->chars, string->length);
                    tea_write_line();
                    POP();
                }
                POP();
                DISPATCH();
            }
            CASE_CODE(GET_LOCAL):
            {
                PUSH(slots[READ_BYTE()]);
                DISPATCH();
            }
            CASE_CODE(SET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                slots[slot] = PEEK(0);
                DISPATCH();
            }
            CASE_CODE(GET_GLOBAL):
            {
                TeaObjectString* name = READ_STRING();
                TeaValue value;
                if(!tea_table_get(&T->globals, name, &value))
                {
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                PUSH(value);
                DISPATCH();
            }
            CASE_CODE(SET_GLOBAL):
            {
                TeaObjectString* name = READ_STRING();
                if(tea_table_set(T, &T->globals, name, PEEK(0)))
                {
                    tea_table_delete(&T->globals, name);
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
                if(tea_table_set(T, &frame->closure->function->module->values, name, PEEK(0)))
                {
                    tea_table_delete(&frame->closure->function->module->values, name);
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                DISPATCH();
            }
            CASE_CODE(DEFINE_OPTIONAL):
            {
                int arity = READ_BYTE();
                int arity_optional = READ_BYTE();
                int arg_count = thread->stack_top - slots - arity_optional - 1;

                // Temp array while we shuffle the stack
                // Cannot have more than 255 args to a function, so
                // we can define this with a constant limit
                TeaValue values[255];
                int index;

                for(index = 0; index < arity_optional + arg_count; index++)
                {
                    values[index] = POP();
                }

                --index;

                for(int i = 0; i < arg_count; i++)
                {
                    PUSH(values[index - i]);
                }

                // Calculate how many "default" values are required
                int remaining = arity + arity_optional - arg_count;

                // Push any "default" values back onto the stack
                for(int i = remaining; i > 0; i--)
                {
                    PUSH(values[i - 1]);
                }
                DISPATCH();
            }
            CASE_CODE(DEFINE_GLOBAL):
            {
                TeaObjectString* name = READ_STRING();
                tea_table_set(T, &T->globals, name, PEEK(0));
                POP();
                DISPATCH();
            }
            CASE_CODE(DEFINE_MODULE):
            {
                TeaObjectString* name = READ_STRING();
                tea_table_set(T, &frame->closure->function->module->values, name, PEEK(0));
                POP();
                DISPATCH();
            }
            CASE_CODE(GET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                PUSH(*upvalues[slot]->location);
                DISPATCH();
            }
            CASE_CODE(SET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                *upvalues[slot]->location = PEEK(0);
                DISPATCH();
            }
            CASE_CODE(GET_PROPERTY):
            {
                TeaValue receiver = PEEK(0);
                TeaObjectString* name = READ_STRING();
                STORE_FRAME;
                if(!get_property(T, receiver, name, true))
                {
                    return TEA_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(GET_PROPERTY_NO_POP):
            {
                TeaValue receiver = PEEK(0);
                TeaObjectString* name = READ_STRING();
                STORE_FRAME;
                if(!get_property(T, receiver, name, false))
                {
                    return TEA_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(SET_PROPERTY):
            {
                TeaObjectString* name = READ_STRING();
                TeaValue receiver = PEEK(1);
                TeaValue item = PEEK(0);
                STORE_FRAME;
                if(!set_property(T, name, receiver, item))
                {
                    return TEA_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(GET_SUPER):
            {
                TeaObjectString* name = READ_STRING();
                TeaObjectClass* superclass = AS_CLASS(POP());

                if(!bind_method(T, superclass, name))
                {
                    return TEA_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(RANGE):
            {
                TeaValue c = POP();
                TeaValue b = POP();
                TeaValue a = POP();

                if(!IS_NUMBER(a) || !IS_NUMBER(b) || !IS_NUMBER(c)) 
                {
                    RUNTIME_ERROR("Range operands must be numbers");
                }

                PUSH(OBJECT_VAL(tea_new_range(T, AS_NUMBER(a), AS_NUMBER(b), AS_NUMBER(c))));
                DISPATCH();
            }
            CASE_CODE(LIST):
            {
                // Stack before: [item1, item2, ..., itemN] and after: [list]
                uint8_t item_count = READ_BYTE();
                TeaObjectList* list = tea_new_list(T);

                PUSH(OBJECT_VAL(list)); // So list isn't sweeped by GC when appending the list
                // Add items to list
                for(int i = item_count; i > 0; i--)
                {
                    if(IS_RANGE(PEEK(i)))
                    {
                        TeaObjectRange* range = AS_RANGE(PEEK(i));

                        int start = range->start;
                        int end = range->end;
                        int step = range->step;

                        if(step > 0)
                        {
                            for(int i = start; i < end; i += step)
                            {
                                tea_write_value_array(T, &list->items, NUMBER_VAL(i));
                            }
                        }
                        else if(step < 0)
                        {
                            for(int i = end + step; i >= 0; i += step)
                            {
                                tea_write_value_array(T, &list->items, NUMBER_VAL(i));
                            }
                        }
                    }
                    else
                    {
                        tea_write_value_array(T, &list->items, PEEK(i));
                    }
                }
                
                // Pop items from stack
                thread->stack_top -= item_count + 1;

                PUSH(OBJECT_VAL(list));
                DISPATCH();
            }
            CASE_CODE(UNPACK_LIST):
            {
                uint8_t var_count = READ_BYTE();

                if(!IS_LIST(PEEK(0)))
                {
                    RUNTIME_ERROR("Can only unpack lists");
                }

                TeaObjectList* list = AS_LIST(POP());

                if(var_count != list->items.count) 
                {
                    if(var_count < list->items.count)
                    {
                        RUNTIME_ERROR("Too many values to unpack");
                    } 
                    else
                    {
                        RUNTIME_ERROR("Not enough values to unpack");
                    }
                }

                for(int i = 0; i < list->items.count; i++)
                {
                    PUSH(list->items.values[i]);
                }

                DISPATCH();
            }
            CASE_CODE(UNPACK_REST_LIST):
            {
                uint8_t var_count = READ_BYTE();
                uint8_t rest_pos = READ_BYTE();

                if(!IS_LIST(PEEK(0)))
                {
                    RUNTIME_ERROR("Can only unpack lists");
                }

                TeaObjectList* list = AS_LIST(POP());

                if(var_count > list->items.count)
                {
                    RUNTIME_ERROR("Not enough values to unpack");
                }

                for(int i = 0; i < list->items.count; i++)
                {
                    if(i == rest_pos)
                    {
                        TeaObjectList* rest_list = tea_new_list(T);
                        PUSH(OBJECT_VAL(rest_list));
                        int j;
                        for(j = i; j < list->items.count - (var_count - rest_pos) + 1; j++)
                        {
                            tea_write_value_array(T, &rest_list->items, list->items.values[j]);
                        }
                        i = j - 1;
                    }
                    else
                    {
                        PUSH(list->items.values[i]);
                    }
                }

                DISPATCH();
            }
            CASE_CODE(MAP):
            {
                uint8_t item_count = READ_BYTE();
                TeaObjectMap* map = tea_new_map(T);

                PUSH(OBJECT_VAL(map));

                for(int i = item_count * 2; i > 0; i -= 2)
                {
                    if(!tea_is_valid_key(PEEK(i)))
                    {
                        RUNTIME_ERROR("Map key isn't hashable");
                    }

                    tea_map_set(T, map, PEEK(i), PEEK(i - 1));
                }

                thread->stack_top -= item_count * 2 + 1;

                PUSH(OBJECT_VAL(map));
                DISPATCH();
            }
            CASE_CODE(SUBSCRIPT):
            {
                // Stack before: [list, index] and after: [index(list, index)]
                TeaValue index = PEEK(0);
                TeaValue list = PEEK(1);
                if(IS_INSTANCE(list))
                {
                    POP();
                    PUSH(index);
                    PUSH(NULL_VAL);             
                    INVOKE_METHOD(list, NULL_VAL, "[]", 2);
                    DISPATCH();
                }
                STORE_FRAME;
                if(!subscript(T, index, list))
                {
                    return TEA_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(SUBSCRIPT_STORE):
            {
                // Stack before list: [list, index, item] and after: [item]
                TeaValue item = PEEK(0);
                TeaValue index = PEEK(1);
                TeaValue list = PEEK(2);
                if(IS_INSTANCE(list))
                {
                    POP();
                    POP();
                    PUSH(index);
                    PUSH(item);
                    INVOKE_METHOD(list, NULL_VAL, "[]", 2);
                    DISPATCH();
                }
                STORE_FRAME;
                if(!subscript_store(T, item, index, list, true))
                {
                    return TEA_RUNTIME_ERROR;
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
                if(!subscript_store(T, item, index, list, false))
                {
                    return TEA_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(SLICE):
            {
                TeaValue step = PEEK(0);
                TeaValue end = PEEK(1);
                TeaValue start = PEEK(2);
                TeaValue object = PEEK(3);
                if(!slice(T, object, start, end, step))
                {
                    return TEA_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(IS):
            {
                TeaValue instance = PEEK(1);
                TeaValue klass = PEEK(0);

                if(!IS_CLASS(klass))
                {
                    RUNTIME_ERROR("Right operand must be a class");
                }

                if(!IS_INSTANCE(instance))
                {
                    DROP_MULTIPLE(2); // Drop the instance and class
                    PUSH(FALSE_VAL);
                    DISPATCH();
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

                    instance_klass = (TeaObjectClass*)instance_klass->super;
                }
                
                DROP_MULTIPLE(2); // Drop the instance and class
                PUSH(BOOL_VAL(found));

                DISPATCH();
            }
            CASE_CODE(IN):
            {
                TeaValue object = PEEK(0);
                TeaValue value = PEEK(1);
                STORE_FRAME;
                if(!in_(T, object, value))
                {
                    return TEA_RUNTIME_ERROR;
                }
                DISPATCH();
            }
            CASE_CODE(EQUAL):
            {
                if(IS_INSTANCE(PEEK(1)) || IS_INSTANCE(PEEK(0)))
                {
                    TeaValue a = PEEK(1);
                    TeaValue b = PEEK(0);
                    DROP();
                    PUSH(a);
                    PUSH(b);
                    INVOKE_METHOD(a, b, "==", 2);
                    DISPATCH();
                }
                
                TeaValue b = POP();
                TeaValue a = POP();
                PUSH(BOOL_VAL(tea_values_equal(a, b)));
                DISPATCH();
            }
            CASE_CODE(GREATER):
            {
                BINARY_OP(BOOL_VAL, >, ">", double);
                DISPATCH();
            }
            CASE_CODE(GREATER_EQUAL):
            {
                BINARY_OP(BOOL_VAL, >=, ">=", double);
                DISPATCH();
            }
            CASE_CODE(LESS):
            {
                BINARY_OP(BOOL_VAL, <, "<", double);
                DISPATCH();
            }
            CASE_CODE(LESS_EQUAL):
            {
                BINARY_OP(BOOL_VAL, <=, "<=", double);
                DISPATCH();
            }
            CASE_CODE(ADD):
            {
                if(IS_STRING(PEEK(0)) && IS_STRING(PEEK(1)))
                {
                    concatenate(T);
                }
                else if(IS_LIST(PEEK(0)) && IS_LIST(PEEK(1)))
                {
                    TeaObjectList* l2 = AS_LIST(PEEK(0));
                    TeaObjectList* l1 = AS_LIST(PEEK(1));

                    for(int i = 0; i < l2->items.count; i++)
                    {
                        tea_write_value_array(T, &l1->items, l2->items.values[i]);
                    }

                    POP();
                    POP();

                    PUSH(OBJECT_VAL(l1));
                }
                else if(IS_MAP(PEEK(0)) && IS_MAP(PEEK(1)))
                {
                    TeaObjectMap* m2 = AS_MAP(PEEK(0));
                    TeaObjectMap* m1 = AS_MAP(PEEK(1));

                    tea_map_add_all(T, m2, m1);

                    POP();
                    POP();

                    PUSH(OBJECT_VAL(m1));
                }
                else
                {
                    BINARY_OP(NUMBER_VAL, +, "+", double);
                }
                DISPATCH();
            }
            CASE_CODE(SUBTRACT):
            {
                BINARY_OP(NUMBER_VAL, -, "-", double);
                DISPATCH();
            }
            CASE_CODE(MULTIPLY):
            {
                if(IS_STRING(PEEK(0)) && IS_NUMBER(PEEK(1)) || IS_NUMBER(PEEK(0)) && IS_STRING(PEEK(1)))
                {
                    repeat(T);
                }
                else
                {
                    BINARY_OP(NUMBER_VAL, *, "*", double);
                }
                DISPATCH();
            }
            CASE_CODE(DIVIDE):
            {
                BINARY_OP(NUMBER_VAL, /, "/", double);
                DISPATCH();
            }
            CASE_CODE(MOD):
            {
                TeaValue a = PEEK(1);
                TeaValue b = PEEK(0);

                if(IS_NUMBER(a) && IS_NUMBER(b))
                {
                    DROP();
                    thread->stack_top[-1] = (NUMBER_VAL(fmod(AS_NUMBER(a), AS_NUMBER(b))));
                    DISPATCH();
                }

                INVOKE_METHOD(a, b, "%", 1);
                DISPATCH();
            }
            CASE_CODE(POW):
            {
                TeaValue a = PEEK(1);
                TeaValue b = PEEK(0);

                if(IS_NUMBER(a) && IS_NUMBER(b))
                {
                    DROP();
                    thread->stack_top[-1] = (NUMBER_VAL(pow(AS_NUMBER(a), AS_NUMBER(b))));
                    DISPATCH();
                }

                INVOKE_METHOD(a, b, "**", 1);
                DISPATCH();
            }
            CASE_CODE(BAND):
            {
                BINARY_OP(NUMBER_VAL, &, "&", int);
                DISPATCH();
            }
            CASE_CODE(BOR):
            {
                BINARY_OP(NUMBER_VAL, |, "|", int);
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
                BINARY_OP(NUMBER_VAL, ^, "^", int);
                DISPATCH();
            }
            CASE_CODE(LSHIFT):
            {
                BINARY_OP(NUMBER_VAL, <<, "<<", int);
                DISPATCH();
            }
            CASE_CODE(RSHIFT):
            {
                BINARY_OP(NUMBER_VAL, >>, ">>", int);
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
                if(IS_INSTANCE(PEEK(0)))
                {
                    TeaValue a = PEEK(0);
                    PUSH(a);
                    PUSH(NULL_VAL);
                    INVOKE_METHOD(a, NULL_VAL, "-", 2);
                    DISPATCH();
                }

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
                if(!call_value(T, PEEK(arg_count), arg_count))
                {
                    return TEA_RUNTIME_ERROR;
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(INVOKE):
            {
                TeaObjectString* method = READ_STRING();
                int arg_count = READ_BYTE();
                STORE_FRAME;
                if(!invoke(T, PEEK(arg_count), method, arg_count))
                {
                    return TEA_RUNTIME_ERROR;
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(SUPER):
            {
                TeaObjectString* method = READ_STRING();
                int arg_count = READ_BYTE();
                STORE_FRAME;
                TeaObjectClass* superclass = AS_CLASS(POP());
                if(!invoke_from_class(T, superclass, method, arg_count))
                {
                    return TEA_RUNTIME_ERROR;
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(CLOSURE):
            {
                TeaObjectFunction* function = AS_FUNCTION(READ_CONSTANT());
                TeaObjectClosure* closure = tea_new_closure(T, function);
                PUSH(OBJECT_VAL(closure));
                
                for(int i = 0; i < closure->upvalue_count; i++)
                {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if(is_local)
                    {
                        closure->upvalues[i] = capture_upvalue(T, frame->slots + index);
                    }
                    else
                    {
                        closure->upvalues[i] = upvalues[index];
                    }
                }
                DISPATCH();
            }
            CASE_CODE(CLOSE_UPVALUE):
            {
                close_upvalues(T, thread->stack_top - 1);
                POP();
                DISPATCH();
            }
            CASE_CODE(RETURN):
            {
                TeaValue result = POP();
                close_upvalues(T, slots);
                STORE_FRAME;
                thread->frame_count--;
                if(thread->frame_count == 0)
                {
                    if(thread->parent == NULL)
                    {
                        POP();
                        return TEA_OK;
                    }
                }

                thread->stack_top = slots;
                PUSH(result);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(CLASS):
            {
                PUSH(OBJECT_VAL(tea_new_class(T, READ_STRING(), NULL)));
                DISPATCH();
            }
            CASE_CODE(SET_CLASS_VAR):
            {
                TeaObjectClass* klass = AS_CLASS(PEEK(1));
                TeaObjectString* key = READ_STRING();

                tea_table_set(T, &klass->statics, key, PEEK(0));
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
                TeaObjectClass* klass = AS_CLASS(PEEK(0));
                if(klass == superclass)
                {
                    RUNTIME_ERROR("A class can't inherit from itself");
                }
                klass->super = superclass;
                
                tea_table_add_all(T, &superclass->methods, &klass->methods);
                tea_table_add_all(T, &superclass->statics, &klass->statics);
                POP();
                DISPATCH();
            }
            CASE_CODE(METHOD):
            {
                define_method(T, READ_STRING());
                DISPATCH();
            }
            CASE_CODE(EXTENSION_METHOD):
            {
                if(!IS_CLASS(PEEK(1)))
                {
                    RUNTIME_ERROR("Cannot assign extension method to %s", tea_value_type(PEEK(1)));
                }
                define_method(T, READ_STRING());
                POP();
                DISPATCH();
            }
            CASE_CODE(IMPORT):
            {
                TeaObjectString* file_name = READ_STRING();
                TeaValue module_value;

                // If we have imported this file already, skip
                if(tea_table_get(&T->modules, file_name, &module_value)) 
                {
                    T->last_module = AS_MODULE(module_value);
                    PUSH(NULL_VAL);
                    DISPATCH();
                }

                char path[PATH_MAX];
                if(!tea_resolve_path(frame->closure->function->module->path->chars, file_name->chars, path))
                {
                    RUNTIME_ERROR("Could not open file \"%s\"", file_name->chars);
                }

                char* source = tea_read_file(T, path);

                if(source == NULL) 
                {
                    RUNTIME_ERROR("Could not open file \"%s\"", file_name->chars);
                }

                TeaObjectString* path_obj = tea_copy_string(T, path, strlen(path));
                TeaObjectModule* module = tea_new_module(T, path_obj);
                module->path = tea_dirname(T, path, strlen(path));
                T->last_module = module;

                TeaObjectFunction* function = tea_compile(T, module, source);

                FREE_ARRAY(T, char, source, strlen(source) + 1);

                if(function == NULL) return TEA_COMPILE_ERROR;
                TeaObjectClosure* closure = tea_new_closure(T, function);
                PUSH(OBJECT_VAL(closure));

                STORE_FRAME;
                call(T, closure, 0);
                READ_FRAME();

                DISPATCH();
            }
            CASE_CODE(IMPORT_VARIABLE):
            {
                PUSH(OBJECT_VAL(T->last_module));
                DISPATCH();
            }
            CASE_CODE(IMPORT_FROM):
            {
                int var_count = READ_BYTE();

                for(int i = 0; i < var_count; i++) 
                {
                    TeaValue module_variable;
                    TeaObjectString* variable = READ_STRING();

                    if(!tea_table_get(&T->last_module->values, variable, &module_variable)) 
                    {
                        RUNTIME_ERROR("%s can't be found in module %s", variable->chars, T->last_module->name->chars);
                    }

                    PUSH(module_variable);
                }

                DISPATCH();
            }
            CASE_CODE(IMPORT_END):
            {
                T->last_module = frame->closure->function->module;
                DISPATCH();
            }
            CASE_CODE(IMPORT_NATIVE):
            {
                int index = READ_BYTE();
                TeaObjectString* file_name = READ_STRING();

                TeaValue module_val;
                // If the module is already imported, skip
                if(tea_table_get(&T->modules, file_name, &module_val))
                {
                    T->last_module = AS_MODULE(module_val);
                    PUSH(module_val);
                    DISPATCH();
                }

                tea_import_native_module(T, index);
                TeaValue module = T->slot[T->top - 1];
                
                PUSH(module);
                if(IS_CLOSURE(module)) 
                {
                    STORE_FRAME;
                    call(T, AS_CLOSURE(module), 0);
                    READ_FRAME();

                    tea_table_get(&T->modules, file_name, &module);
                    T->last_module = AS_MODULE(module);
                }

                DISPATCH();
            }
            CASE_CODE(IMPORT_NATIVE_VARIABLE):
            {
                TeaObjectString* file_name = READ_STRING();
                int var_count = READ_BYTE();

                TeaObjectModule* module;

                TeaValue module_val;
                if(tea_table_get(&T->modules, file_name, &module_val)) 
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
    }

    return TEA_RUNTIME_ERROR;
}
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

TeaInterpretResult tea_interpret_module(TeaState* T, const char* module_name, const char* source)
{
    TeaObjectString* name = tea_copy_string(T, module_name, strlen(module_name));
    tea_push_root(T, OBJECT_VAL(name));
    TeaObjectModule* module = tea_new_module(T, name);
    tea_pop_root(T);

    tea_push_root(T, OBJECT_VAL(module));
    module->path = tea_get_directory(T, (char*)module_name);
    tea_pop_root(T);
    
    TeaObjectFunction* function = tea_compile(T, module, source);
    if(function == NULL)
        return TEA_COMPILE_ERROR;

    tea_push_root(T, OBJECT_VAL(function));
    TeaObjectClosure* closure = tea_new_closure(T, function);
    tea_pop_root(T);

    tea_push_root(T, OBJECT_VAL(closure));
    TeaObjectThread* thread = tea_new_thread(T, closure);
    tea_pop_root(T);

    return run_interpreter(T, thread);
}