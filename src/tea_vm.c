/*
** tea_vm.c
** Teascript virtual machine
*/

#define tea_vm_c
#define TEA_CORE

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tea_def.h"
#include "tea_obj.h"
#include "tea_func.h"
#include "tea_map.h"
#include "tea_str.h"
#include "tea_gc.h"
#include "tea_vm.h"
#include "tea_utf.h"
#include "tea_import.h"
#include "tea_err.h"
#include "tea_bc.h"
#include "tea_tab.h"

static bool vm_callT(tea_State* T, GCfuncT* func, int arg_count)
{
    if(arg_count < func->proto->arity)
    {
        if((arg_count + func->proto->variadic) == func->proto->arity)
        {
            /* Add missing variadic param ([]) */
            GClist* list = tea_obj_new_list(T);
            tea_vm_push(T, OBJECT_VAL(list));
            arg_count++;
        }
        else
        {
            tea_err_run(T, "Expected %d arguments, but got %d", func->proto->arity, arg_count);
        }
    }
    else if(arg_count > func->proto->arity + func->proto->arity_optional)
    {
        if(func->proto->variadic)
        {
            int arity = func->proto->arity + func->proto->arity_optional;
            /* +1 for the variadic param itself */
            int varargs = arg_count - arity + 1;
            GClist* list = tea_obj_new_list(T);
            tea_vm_push(T, OBJECT_VAL(list));
            for(int i = varargs; i > 0; i--)
            {
                tea_list_append(T, list, tea_vm_peek(T, i));
            }
            /* +1 for the list pushed earlier on the stack */
            T->top -= varargs + 1;
            tea_vm_push(T, OBJECT_VAL(list));
            arg_count = arity;
        }
        else
        {
            tea_err_run(T, "Expected %d arguments, but got %d", func->proto->arity + func->proto->arity_optional, arg_count);
        }
    }
    else if(func->proto->variadic)
    {
        /* Last argument is the variadic arg */
        GClist* list = tea_obj_new_list(T);
        tea_vm_push(T, OBJECT_VAL(list));
        tea_list_append(T, list, tea_vm_peek(T, 1));
        T->top -= 2;
        tea_vm_push(T, OBJECT_VAL(list));
    }
    
    tea_state_growci(T);
    tea_state_checkstack(T, func->proto->max_slots);

    CallInfo* ci = ++T->ci;
    ci->func = func;
    ci->cfunc = NULL;
    ci->ip = func->proto->code;
    ci->state = CIST_TEA;
    ci->base = T->top - arg_count - 1;

    return true;
}

static bool vm_callC(tea_State* T, GCfuncC* cfunc, int arg_count)
{
    int extra = cfunc->type > C_FUNCTION;
    if((cfunc->nargs != TEA_VARARGS) && ((arg_count + extra) != cfunc->nargs))
    {
        tea_err_run(T, "Expected %d arguments, but got %d", cfunc->nargs, arg_count + extra);
    }

    tea_state_growci(T);
    tea_state_checkstack(T, BASE_STACK_SIZE);

    CallInfo* ci = ++T->ci;
    ci->func = NULL;
    ci->cfunc = cfunc;
    ci->ip = NULL;
    ci->state = CIST_C;
    ci->base = T->top - arg_count - 1;

    if(extra)
        T->base = T->top - arg_count - 1;
    else 
        T->base = T->top - arg_count;

    cfunc->fn(T);
    
    Value res = T->top[-1];

    ci = T->ci--;

    T->base = ci->base;
    T->top = ci->base;
    tea_vm_push(T, res);

    return false;
}

bool vm_precall(tea_State* T, Value callee, uint8_t arg_count)
{
    if(IS_OBJECT(callee))
    {
        switch(OBJECT_TYPE(callee))
        {
            case OBJ_METHOD:
            {
                GCmethod* bound = AS_METHOD(callee);
                T->top[-arg_count - 1] = bound->receiver;
                return vm_precall(T, bound->method, arg_count);
            }
            case OBJ_CLASS:
            {
                GCclass* klass = AS_CLASS(callee);
                T->top[-arg_count - 1] = OBJECT_VAL(tea_obj_new_instance(T, klass));
                if(!IS_NULL(klass->constructor)) 
                {
                    return vm_precall(T, klass->constructor, arg_count);
                }
                else if(arg_count != 0)
                {
                    tea_err_run(T, "Expected 0 arguments but got %d", arg_count);
                }
                return false;
            }
            case OBJ_FUNC:
                return vm_callT(T, AS_FUNC(callee), arg_count);
            case OBJ_CFUNC:
                return vm_callC(T, AS_CFUNC(callee), arg_count);
            default:
                break; /* Non-callable object type */
        }
    }

    tea_err_run(T, "'%s' is not callable", tea_val_type(callee));
    return false;
}

static bool vm_invoke_from_class(tea_State* T, GCclass* klass, GCstr* name, int arg_count)
{
    Value method;
    if(!tea_tab_get(&klass->methods, name, &method))
    {
        tea_err_run(T, "Undefined method '%s'", name->chars);
    }

    return vm_precall(T, method, arg_count);
}

static bool vm_invoke(tea_State* T, Value receiver, GCstr* name, int arg_count)
{
    if(!IS_OBJECT(receiver))
    {
        tea_err_run(T, "Only objects have methods, %s given", tea_val_type(receiver));
    }

    switch(OBJECT_TYPE(receiver))
    {
        case OBJ_MODULE:
        {
            GCmodule* module = AS_MODULE(receiver);

            Value value;
            if(tea_tab_get(&module->values, name, &value))
            {
                return vm_precall(T, value, arg_count);
            }

            tea_err_run(T, "Undefined variable '%s' in '%s' module", name->chars, module->name->chars);
        }
        case OBJ_INSTANCE:
        {
            GCinstance* instance = AS_INSTANCE(receiver);

            Value value;
            if(tea_tab_get(&instance->fields, name, &value))
            {
                T->top[-arg_count - 1] = value;
                return vm_precall(T, value, arg_count);
            }

            return vm_invoke_from_class(T, instance->klass, name, arg_count);
        }
        case OBJ_CLASS:
        {
            GCclass* klass = AS_CLASS(receiver);
            Value method;
            if(tea_tab_get(&klass->methods, name, &method))
            {
                if(IS_CFUNC(method) || AS_FUNC(method)->proto->type != PROTO_STATIC)
                {
                    tea_err_run(T, "'%s' is not static. Only static methods can be invoked directly from a class", name->chars);
                }

                return vm_precall(T, method, arg_count);
            }

            tea_err_run(T, "Undefined method '%s'", name->chars);
        }
        default:
        {
            GCclass* type = tea_state_get_class(T, receiver);
            if(type != NULL)
            {
                Value value;
                if(tea_tab_get(&type->methods, name, &value))
                {
                    return vm_precall(T, value, arg_count);
                }

                tea_err_run(T, "%s has no method %s()", tea_val_type(receiver), name->chars);
            }
        }
    }
    return false;
}

static bool vm_bind_method(tea_State* T, GCclass* klass, GCstr* name)
{
    Value method;
    if(!tea_tab_get(&klass->methods, name, &method))
    {
        tea_err_run(T, "Undefined method '%s'", name->chars);
    }

    GCmethod* bound = tea_obj_new_method(T, tea_vm_peek(T, 0), method);
    tea_vm_pop(T, 1);
    tea_vm_push(T, OBJECT_VAL(bound));
    return true;
}

static bool vm_slice(tea_State* T, Value object, Value start_index, Value end_index, Value step_index)
{
    if(!IS_NUMBER(step_index) || (!IS_NUMBER(end_index) && !IS_NULL(end_index)) || !IS_NUMBER(step_index))
    {
        tea_err_run(T, "Slice index must be a number");
        return false;
    }

    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_LIST:
            {
                GClist* new_list = tea_obj_new_list(T);
                tea_vm_push(T, OBJECT_VAL(new_list));
                GClist* list = AS_LIST(object);

                int start = AS_NUMBER(start_index);
                int end;
                int step = AS_NUMBER(step_index);

                if(IS_NULL(end_index))
                {
                    end = list->count;
                }
                else
                {
                    end = AS_NUMBER(end_index);
                    if(end > list->count)
                    {
                        end = list->count;
                    }
                    else if(end < 0)
                    {
                        end = list->count + end;
                    }
                }

                if(step > 0)
                {
                    for(int i = start; i < end; i += step)
                    {
                        tea_list_append(T, new_list, list->items[i]);
                    }
                }
                else if(step < 0)
                {
                    for(int i = end + step; i >= start; i += step)
                    {
                        tea_list_append(T, new_list, list->items[i]);
                    }
                }

                tea_vm_pop(T, 5);   /* +1 for the pushed list */
                tea_vm_push(T, OBJECT_VAL(new_list));
                return true;
            }
            case OBJ_STRING:
            {
                GCstr* string = AS_STRING(object);
                int len = tea_utf_len(string);

                int start = AS_NUMBER(start_index);
                int end;

                if(IS_NULL(end_index))
                {
                    end = string->len;
                }
                else
                {
                    end = AS_NUMBER(end_index);
                    if(end > len)
                    {
                        end = len;
                    }
                    else if(end < 0)
                    {
                        end = len + end;
                    }
                }

                /* Ensure the start index is below the end index */
                if(start > end)
                {
                    tea_vm_pop(T, 4);
                    tea_vm_push(T, OBJECT_VAL(tea_str_lit(T, "")));
                    return true;
                }
                else
                {
                    start = tea_utf_char_offset(string->chars, start);
                    end = tea_utf_char_offset(string->chars, end);
                    tea_vm_pop(T, 4);
                    tea_vm_push(T, OBJECT_VAL(tea_utf_from_range(T, string, start, end - start, 1)));
                    return true;
                }
            }
            default:
                break;
        }
    }

    tea_err_run(T, "%s is not sliceable", tea_val_type(object));
    return false;
}

static void vm_subscript(tea_State* T, Value index_value, Value subscript_value)
{
    if(!IS_OBJECT(subscript_value))
    {
        tea_err_run(T, "%s is not subscriptable", tea_val_type(subscript_value));
    }

    switch(OBJECT_TYPE(subscript_value))
    {
        case OBJ_INSTANCE:
        {
            GCinstance* instance = AS_INSTANCE(subscript_value);

            GCstr* subs = T->opm_name[MM_INDEX];

            Value method;
            if(tea_tab_get(&instance->klass->methods, subs, &method))
            {
                tea_vm_push(T, NULL_VAL);
                tea_vm_call(T, method, 2);
                return;
            }

            tea_err_run(T, "'%s' instance is not subscriptable", instance->klass->name);
        }
        case OBJ_RANGE:
        {
            if(!IS_NUMBER(index_value))
            {
                tea_err_run(T, "Range index must be a number");
            }

            GCrange* range = AS_RANGE(subscript_value);
            double index = AS_NUMBER(index_value);

            /* Calculate the length of the range */
            double len = (range->end - range->start) / range->step;

            /* Allow negative indexes */
            if(index < 0)
            {
                index = len + index;
            }

            if(index >= 0 && index < len)
            {
                tea_vm_pop(T, 2);
                tea_vm_push(T, NUMBER_VAL(range->start + index * range->step));
                return;
            }

            tea_err_run(T, "Range index out of bounds");
        }
        case OBJ_LIST:
        {
            if(!IS_NUMBER(index_value))
            {
                tea_err_run(T, "List index must be a number");
            }

            GClist* list = AS_LIST(subscript_value);
            int index = AS_NUMBER(index_value);

            /* Allow negative indexes */
            if(index < 0)
            {
                index = list->count + index;
            }

            if(index >= 0 && index < list->count)
            {
                tea_vm_pop(T, 2);
                tea_vm_push(T, list->items[index]);
                return;
            }

            tea_err_run(T, "List index out of bounds");
        }
        case OBJ_MAP:
        {
            GCmap* map = AS_MAP(subscript_value);
            if(!tea_map_hashable(index_value))
            {
                tea_err_run(T, "Map key isn't hashable");
            }

            Value value;
            tea_vm_pop(T, 2);
            if(tea_map_get(map, index_value, &value))
            {
                tea_vm_push(T, value);
                return;
            }

            tea_err_run(T, "Key does not exist within map");
        }
        case OBJ_STRING:
        {
            if(!IS_NUMBER(index_value))
            {
                tea_err_run(T, "String index must be a number (got %s)", tea_val_type(index_value));
            }

            GCstr* string = AS_STRING(subscript_value);
            int index = AS_NUMBER(index_value);
            int real_len = tea_utf_len(string);

            /* Allow negative indexes */
            if(index < 0)
            {
                index = real_len + index;
            }

            if(index >= 0 && index < string->len)
            {
                tea_vm_pop(T, 2);
                GCstr* c = tea_utf_codepoint_at(T, string, tea_utf_char_offset(string->chars, index));
                tea_vm_push(T, OBJECT_VAL(c));
                return;
            }

            tea_err_run(T, "String index out of bounds");
        }
        default:
            break;
    }

    tea_err_run(T, "'%s' is not subscriptable", tea_val_type(subscript_value));
}

static void vm_subscript_store(tea_State* T, Value item_value, Value index_value, Value subscript_value, bool assign)
{
    if(!IS_OBJECT(subscript_value))
    {
        tea_err_run(T, "'%s' is not subscriptable", tea_val_type(subscript_value));
    }

    switch(OBJECT_TYPE(subscript_value))
    {
        case OBJ_INSTANCE:
        {
            GCinstance* instance = AS_INSTANCE(subscript_value);

            GCstr* subs = T->opm_name[MM_INDEX];

            Value method;
            if(tea_tab_get(&instance->klass->methods, subs, &method))
            {
                tea_vm_call(T, method, 2);
                return;
            }

            tea_err_run(T, "'%s' does not support item assignment", instance->klass->name);
        }
        case OBJ_LIST:
        {
            if(!IS_NUMBER(index_value))
            {
                tea_err_run(T, "List index must be a number (got %s)", tea_val_type(index_value));
            }

            GClist* list = AS_LIST(subscript_value);
            int index = AS_NUMBER(index_value);

            if(index < 0)
            {
                index = list->count + index;
            }

            if(index >= 0 && index < list->count)
            {
                if(assign)
                {
                    list->items[index] = item_value;
                    tea_vm_pop(T, 3);
                    tea_vm_push(T, item_value);
                }
                else
                {
                    T->top[-1] = list->items[index];
                    tea_vm_push(T, item_value);
                }
                return;
            }

            tea_err_run(T, "List index out of bounds");
        }
        case OBJ_MAP:
        {
            GCmap* map = AS_MAP(subscript_value);
            if(!tea_map_hashable(index_value))
            {
                tea_err_run(T, "Map key isn't hashable");
            }

            if(assign)
            {
                tea_map_set(T, map, index_value, item_value);
                tea_vm_pop(T, 3);
                tea_vm_push(T, item_value);
            }
            else
            {
                Value map_value;
                if(!tea_map_get(map, index_value, &map_value))
                {
                    tea_err_run(T, "Key does not exist within the map");
                }
                T->top[-1] = map_value;
                tea_vm_push(T, item_value);
            }
            return;
        }
        default:
            break;
    }

    tea_err_run(T, "'%s' does not support item assignment", tea_val_type(subscript_value));
}

static void vm_get_property(tea_State* T, Value receiver, GCstr* name, bool dopop)
{
    if(!IS_OBJECT(receiver))
    {
        tea_err_run(T, "Only objects have properties");
    }

    switch(OBJECT_TYPE(receiver))
    {
        case OBJ_INSTANCE:
        {
            GCinstance* instance = AS_INSTANCE(receiver);

            Value value;
            if(tea_tab_get(&instance->fields, name, &value))
            {
                if(dopop)
                {
                    tea_vm_pop(T, 1); /* Instance */
                }
                tea_vm_push(T, value);
                return;
            }

            if(vm_bind_method(T, instance->klass, name))
                return;

            GCclass* klass = instance->klass;
            while(klass != NULL)
            {
                if(tea_tab_get(&klass->statics, name, &value))
                {
                    if(dopop)
                    {
                        tea_vm_pop(T, 1); /* Instance */
                    }
                    tea_vm_push(T, value);
                    return;
                }

                klass = klass->super;
            }

            tea_err_run(T, "'%s' instance has no property: '%s'", instance->klass->name->chars, name->chars);
        }
        case OBJ_CLASS:
        {
            GCclass* klass = AS_CLASS(receiver);
            GCclass* klass_store = klass;

            while(klass != NULL)
            {
                Value value;
                if(tea_tab_get(&klass->statics, name, &value))
                {
                    if(dopop)
                    {
                        tea_vm_pop(T, 1); /* Class */
                    }
                    tea_vm_push(T, value);
                    return;
                }

                klass = klass->super;
            }

            tea_err_run(T, "'%s' class has no property: '%s'", klass_store->name->chars, name->chars);
        }
        case OBJ_MODULE:
        {
            GCmodule* module = AS_MODULE(receiver);

            Value value;
            if(tea_tab_get(&module->values, name, &value))
            {
                if(dopop)
                {
                    tea_vm_pop(T, 1); /* Module */
                }
                tea_vm_push(T, value);
                return;
            }

            tea_err_run(T, "'%s' module has no property: '%s'", module->name->chars, name->chars);
        }
        case OBJ_MAP:
        {
            GCmap* map = AS_MAP(receiver);

            Value value;
            if(tea_map_get(map, OBJECT_VAL(name), &value))
            {
                if(dopop)
                {
                    tea_vm_pop(T, 1);
                }
                tea_vm_push(T, value);
                return;
            }
            else
            {
                goto retry;
            }

            tea_err_run(T, "Map has no property: '%s'", name->chars);
        }
        default:
        retry:
        {
            GCclass* klass = tea_state_get_class(T, receiver);
            if(klass != NULL)
            {
                Value value;
                if(tea_tab_get(&klass->methods, name, &value))
                {
                    if(IS_CFUNC(value) && AS_CFUNC(value)->type == C_PROPERTY)
                    {
                        if(!dopop)
                        {
                            tea_vm_push(T, receiver);
                        }
                        tea_vm_call(T, value, 0);
                    }
                    else
                    {
                        vm_bind_method(T, klass, name);
                    }
                    return;
                }
            }
            break;
        }
    }

    tea_err_run(T, "'%s' has no property '%s'", tea_val_type(receiver), name->chars);
}

static void vm_set_property(tea_State* T, GCstr* name, Value receiver, Value item)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_INSTANCE:
            {
                GCinstance* instance = AS_INSTANCE(receiver);
                tea_tab_set(T, &instance->fields, name, item);
                tea_vm_pop(T, 2);
                tea_vm_push(T, item);
                return;
            }
            case OBJ_CLASS:
            {
                GCclass* klass = AS_CLASS(receiver);
                tea_tab_set(T, &klass->statics, name, item);
                tea_vm_pop(T, 2);
                tea_vm_push(T, item);
                return;
            }
            case OBJ_MAP:
            {
                GCmap* map = AS_MAP(receiver);
                tea_map_set(T, map, OBJECT_VAL(name), item);
                tea_vm_pop(T, 2);
                tea_vm_push(T, item);
                return;
            }
            case OBJ_MODULE:
            {
                GCmodule* module = AS_MODULE(receiver);
                tea_tab_set(T, &module->values, name, item);
                tea_vm_pop(T, 2);
                tea_vm_push(T, item);
                return;
            }
            default:
            {
                GCclass* klass = tea_state_get_class(T, receiver);
                if(klass != NULL)
                {
                    Value value;
                    if(tea_tab_get(&klass->methods, name, &value))
                    {
                        if(IS_CFUNC(value) && AS_CFUNC(value)->type == C_PROPERTY)
                        {
                            tea_vm_call(T, value, 1);
                            return;
                        }
                    }
                }
                break;
            }
        }
    }

    tea_err_run(T, "Cannot set property on type '%s'", tea_val_type(receiver));
}

static void vm_define_method(tea_State* T, GCstr* name)
{
    Value method = tea_vm_peek(T, 0);
    GCclass* klass = AS_CLASS(tea_vm_peek(T, 1));
    tea_tab_set(T, &klass->methods, name, method);
    if(name == T->constructor_string) klass->constructor = method;
    tea_vm_pop(T, 1);
}

static bool vm_get_opmethod(tea_State* T, GCstr* key, Value v, Value* method)
{
    GCclass* klass = tea_state_get_class(T, v);
    if(klass == NULL)
        return false;

    return tea_tab_get(&klass->methods, key, method);
}

static void vm_unary_arith(tea_State* T, MMS op, Value v)
{
    GCstr* method_name = T->opm_name[op];

    Value method;
    if(!vm_get_opmethod(T, method_name, v, &method))
    {
        tea_err_run(T, "Attempt to use '%s' unary operator with %s", method_name->chars, tea_val_type(v));
    }

    tea_vm_pop(T, 1);
    tea_vm_push(T, method);
    tea_vm_push(T, v);
    tea_vm_push(T, NULL_VAL);
    tea_vm_call(T, method, 2);
}

static void vm_arith(tea_State* T, MMS op, Value a, Value b)
{
    GCstr* method_name = T->opm_name[op];

    Value method;
    bool found = vm_get_opmethod(T, method_name, a, &method);    /* try first operand */
    if(!found)
    {
        found = vm_get_opmethod(T, method_name, b, &method); /* try second operand */
    }
    if(!found)
    {
        tea_err_run(T, "Attempt to use '%s' operator with %s and %s", method_name->chars, tea_val_type(a), tea_val_type(b));
    }

    tea_vm_pop(T, 2);
    tea_vm_push(T, method);
    tea_vm_push(T, a);
    tea_vm_push(T, b);
    tea_vm_call(T, method, 2);
}

static bool vm_arith_comp(tea_State* T, MMS op, Value a, Value b)
{
    GCstr* method_name = T->opm_name[op];

    Value method;
    bool found = vm_get_opmethod(T, method_name, a, &method);    /* try first operand */
    if(!found)
    {
        found = vm_get_opmethod(T, method_name, b, &method); /* try second operand */
    }
    if(!found)
    {
        return false;
    }

    tea_vm_pop(T, 2);
    tea_vm_push(T, method);
    tea_vm_push(T, a);
    tea_vm_push(T, b);
    tea_vm_call(T, method, 2);
    return true;
}

static bool vm_iterator_call(tea_State* T, MMS op, Value receiver)
{
    GCstr* method_name = T->opm_name[op];

    Value method;
    GCclass* klass = tea_state_get_class(T, receiver);
    if(klass == NULL)
        return false;

    if(tea_tab_get(&klass->methods, method_name, &method))
    {
        tea_vm_call(T, method, 1);
        return true;
    }
    return false;
}

static void vm_execute(tea_State* T)
{
#define PUSH(value) (tea_vm_push(T, value))
#define POP() (tea_vm_pop(T, 1))
#define PEEK(distance) (tea_vm_peek(T, distance))
#define DROP(amount) (T->top -= (amount))
#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

#define STORE_FRAME (T->ci->ip = ip)
#define READ_FRAME() \
    do \
    { \
	    ip = T->ci->ip; \
	    base = T->ci->base; \
    } \
    while(false)

#define READ_CONSTANT() (T->ci->func->proto->k[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define RUNTIME_ERROR(...) \
    do \
    { \
        STORE_FRAME; \
        tea_err_run(T, __VA_ARGS__); \
        READ_FRAME(); \
        DISPATCH(); \
    } \
    while(false)

#define BINARY_OP(value_type, expr, op_method, type) \
    do \
    { \
        Value v1 = PEEK(1); \
        Value v2 = PEEK(0); \
        if(IS_NUMBER(v1) && IS_NUMBER(v2)) \
        { \
            type b = AS_NUMBER(POP()); \
            type a = AS_NUMBER(PEEK(0)); \
            T->top[-1] = value_type(expr); \
        } \
        else \
        { \
            STORE_FRAME; \
            vm_arith(T, op_method, v1, v2); \
            READ_FRAME(); \
        } \
    } \
    while(false)

#define UNARY_OP(value_type, expr, op_method, type) \
    do \
    { \
        Value v1 = PEEK(0); \
        if(IS_NUMBER(v1)) \
        { \
            type v = AS_NUMBER(v1); \
            T->top[-1] = value_type(expr); \
        } \
        else \
        { \
            STORE_FRAME; \
            vm_unary_arith(T, op_method, v1); \
            READ_FRAME(); \
        } \
    } \
    while(false)

#ifdef TEA_COMPUTED_GOTO
    static void* dispatch_table[] = {
        #define BCGOTO(name, _) &&BC_##name,
        BCDEF(BCGOTO)
        #undef BCGOTO
    };

    #define DISPATCH() \
        do \
        { \
            goto *dispatch_table[instruction = READ_BYTE()]; \
        } \
        while(false)

    #define INTERPRET_LOOP  DISPATCH();
    #define CASE_CODE(name) name
#else
    #define INTERPRET_LOOP \
        loop: \
            switch(instruction = READ_BYTE())

    #define DISPATCH() goto loop

    #define CASE_CODE(name) case name
#endif

    uint8_t* ip;
    Value* base;

    READ_FRAME();
    (T->ci - 1)->state = CIST_REENTRY;

    /* Main interpreter loop */
    while(true)
    {
        uint8_t instruction;
        INTERPRET_LOOP
        {
            CASE_CODE(BC_CONSTANT):
            {
                PUSH(READ_CONSTANT());
                DISPATCH();
            }
            CASE_CODE(BC_NULL):
            {
                PUSH(NULL_VAL);
                DISPATCH();
            }
            CASE_CODE(BC_TRUE):
            {
                PUSH(TRUE_VAL);
                DISPATCH();
            }
            CASE_CODE(BC_FALSE):
            {
                PUSH(FALSE_VAL);
                DISPATCH();
            }
            CASE_CODE(BC_POP):
            {
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_PRINT):
            {
                Value value = PEEK(0);
                if(!IS_NULL(value))
                {
                    tea_tab_set(T, &T->globals, T->repl_string, value);
                    GCstr* string = tea_val_tostring(T, value, 0);
                    PUSH(OBJECT_VAL(string));
                    fwrite(string->chars, sizeof(char), string->len, stdout);
                    putchar('\n');
                    DROP(1);
                }
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_GET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                PUSH(base[slot]);
                DISPATCH();
            }
            CASE_CODE(BC_SET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                base[slot] = PEEK(0);
                DISPATCH();
            }
            CASE_CODE(BC_GET_GLOBAL):
            {
                GCstr* name = READ_STRING();
                Value value;
                if(!tea_tab_get(&T->globals, name, &value))
                {
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                PUSH(value);
                DISPATCH();
            }
            CASE_CODE(BC_SET_GLOBAL):
            {
                GCstr* name = READ_STRING();
                if(tea_tab_set(T, &T->globals, name, PEEK(0)))
                {
                    tea_tab_delete(&T->globals, name);
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                DISPATCH();
            }
            CASE_CODE(BC_GET_MODULE):
            {
                GCstr* name = READ_STRING();
                Value value;
                if(!tea_tab_get(&T->ci->func->proto->module->values, name, &value))
                {
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                PUSH(value);
                DISPATCH();
            }
            CASE_CODE(BC_SET_MODULE):
            {
                GCstr* name = READ_STRING();
                if(tea_tab_set(T, &T->ci->func->proto->module->values, name, PEEK(0)))
                {
                    tea_tab_delete(&T->ci->func->proto->module->values, name);
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                DISPATCH();
            }
            CASE_CODE(BC_DEFINE_OPTIONAL):
            {
                int arity = READ_BYTE();
                int arity_optional = READ_BYTE();
                int arg_count = T->top - base - arity_optional - 1;

                /*
                ** Temp array while we shuffle the stack
                ** Cannot have more than 255 args to a function, so
                ** we can define this with a constant limit
                */
                Value values[255];
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

                /* Calculate how many "default" values are required */
                int remaining = arity + arity_optional - arg_count;

                /* Push any "default" values back onto the stack */
                for(int i = remaining; i > 0; i--)
                {
                    PUSH(values[i - 1]);
                }
                DISPATCH();
            }
            CASE_CODE(BC_DEFINE_GLOBAL):
            {
                GCstr* name = READ_STRING();
                tea_tab_set(T, &T->globals, name, PEEK(0));
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_DEFINE_MODULE):
            {
                GCstr* name = READ_STRING();
                tea_tab_set(T, &T->ci->func->proto->module->values, name, PEEK(0));
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_GET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                PUSH(*T->ci->func->upvalues[slot]->location);
                DISPATCH();
            }
            CASE_CODE(BC_SET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                *T->ci->func->upvalues[slot]->location = PEEK(0);
                DISPATCH();
            }
            CASE_CODE(BC_GET_PROPERTY):
            {
                Value receiver = PEEK(0);
                GCstr* name = READ_STRING();
                STORE_FRAME;
                vm_get_property(T, receiver, name, true);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_GET_PROPERTY_NO_POP):
            {
                Value receiver = PEEK(0);
                GCstr* name = READ_STRING();
                STORE_FRAME;
                vm_get_property(T, receiver, name, false);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SET_PROPERTY):
            {
                GCstr* name = READ_STRING();
                Value receiver = PEEK(1);
                Value item = PEEK(0);
                STORE_FRAME;
                vm_set_property(T, name, receiver, item);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_GET_SUPER):
            {
                GCstr* name = READ_STRING();
                GCclass* superclass = AS_CLASS(POP());
                STORE_FRAME;
                vm_bind_method(T, superclass, name);
                DISPATCH();
            }
            CASE_CODE(BC_RANGE):
            {
                Value c = POP();
                Value b = POP();
                Value a = POP();

                if(!IS_NUMBER(a) || !IS_NUMBER(b) || !IS_NUMBER(c))
                {
                    RUNTIME_ERROR("Range operands must be numbers");
                }

                PUSH(OBJECT_VAL(tea_obj_new_range(T, AS_NUMBER(a), AS_NUMBER(b), AS_NUMBER(c))));
                DISPATCH();
            }
            CASE_CODE(BC_LIST):
            {
                GClist* list = tea_obj_new_list(T);
                PUSH(OBJECT_VAL(list));
                DISPATCH();
            }
            CASE_CODE(BC_UNPACK):
            {
                uint8_t var_count = READ_BYTE();

                if(!IS_LIST(PEEK(0)))
                {
                    RUNTIME_ERROR("Can only unpack lists");
                }

                GClist* list = AS_LIST(POP());

                if(var_count != list->count)
                {
                    if(var_count < list->count)
                    {
                        RUNTIME_ERROR("Too many values to unpack");
                    }
                    else
                    {
                        RUNTIME_ERROR("Not enough values to unpack");
                    }
                }

                for(int i = 0; i < list->count; i++)
                {
                    PUSH(list->items[i]);
                }

                DISPATCH();
            }
            CASE_CODE(BC_UNPACK_REST):
            {
                uint8_t var_count = READ_BYTE();
                uint8_t rest_pos = READ_BYTE();

                if(!IS_LIST(PEEK(0)))
                {
                    RUNTIME_ERROR("Can only unpack lists");
                }

                GClist* list = AS_LIST(POP());

                if(var_count > list->count)
                {
                    RUNTIME_ERROR("Not enough values to unpack");
                }

                for(int i = 0; i < list->count; i++)
                {
                    if(i == rest_pos)
                    {
                        GClist* rest_list = tea_obj_new_list(T);
                        PUSH(OBJECT_VAL(rest_list));
                        int j;
                        for(j = i; j < list->count - (var_count - rest_pos) + 1; j++)
                        {
                            tea_list_append(T, rest_list, list->items[j]);
                        }
                        i = j - 1;
                    }
                    else
                    {
                        PUSH(list->items[i]);
                    }
                }

                DISPATCH();
            }
            CASE_CODE(BC_MAP):
            {
                GCmap* map = tea_map_new(T);
                PUSH(OBJECT_VAL(map));
                DISPATCH();
            }
            CASE_CODE(BC_SUBSCRIPT):
            {
                Value list = PEEK(1);
                Value index = PEEK(0);
                STORE_FRAME;
                vm_subscript(T, index, list);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SUBSCRIPT_STORE):
            {
                Value list = PEEK(2);
                Value index = PEEK(1);
                Value item = PEEK(0);
                STORE_FRAME;
                vm_subscript_store(T, item, index, list, true);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SUBSCRIPT_PUSH):
            {
                Value list = PEEK(2);
                Value index = PEEK(1);
                Value item = PEEK(0);
                STORE_FRAME;
                vm_subscript_store(T, item, index, list, false);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SLICE):
            {
                Value object = PEEK(3);
                Value start = PEEK(2);
                Value end = PEEK(1);
                Value step = PEEK(0);
                STORE_FRAME;
                vm_slice(T, object, start, end, step);
                DISPATCH();
            }
            CASE_CODE(BC_LIST_ITEM):
            {
                GClist* list = AS_LIST(PEEK(1));
                Value item = PEEK(0);

                if(IS_RANGE(item))
                {
                    GCrange* range = AS_RANGE(item);

                    int start = range->start;
                    int end = range->end;
                    int step = range->step;

                    if(step > 0)
                    {
                        for(int i = start; i < end; i += step)
                        {
                            tea_list_append(T, list, NUMBER_VAL(i));
                        }
                    }
                    else if(step < 0)
                    {
                        for(int i = end + step; i >= 0; i += step)
                        {
                            tea_list_append(T, list, NUMBER_VAL(i));
                        }
                    }
                }
                else
                {
                    tea_list_append(T, list, item);
                }

                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_MAP_FIELD):
            {
                GCmap* map = AS_MAP(PEEK(2));
                Value key = PEEK(1);
                Value value = PEEK(0);

                if(!tea_map_hashable(key))
                {
                    RUNTIME_ERROR("%s isn't a hashable map key", tea_val_type(key));
                }

                tea_map_set(T, map, key, value);
                DROP(2);
                DISPATCH();
            }
            CASE_CODE(BC_IS):
            {
                Value instance = PEEK(1);
                Value klass = PEEK(0);

                if(!IS_CLASS(klass))
                {
                    RUNTIME_ERROR("Right operand must be a class");
                }

                GCclass* instance_klass = tea_state_get_class(T, instance);
                if(instance_klass == NULL)
                {
                    DROP(2); /* Drop the instance and class */
                    PUSH(FALSE_VAL);
                    DISPATCH();
                }

                GCclass* type = AS_CLASS(klass);
                bool found = false;

                while(instance_klass != NULL)
                {
                    if(instance_klass == type)
                    {
                        found = true;
                        break;
                    }

                    instance_klass = (GCclass*)instance_klass->super;
                }

                DROP(2); /* Drop the instance and class */
                PUSH(BOOL_VAL(found));
                DISPATCH();
            }
            CASE_CODE(BC_IN):
            {
                Value value = PEEK(1);
                Value object = PEEK(0);
                DROP(2);
                PUSH(object); PUSH(value);
                STORE_FRAME;
                if(!vm_iterator_call(T, MM_CONTAINS, object))
                {
                    RUNTIME_ERROR("'%s' is not iterable", tea_val_type(object));
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_EQUAL):
            {
                Value a = PEEK(1);
                Value b = PEEK(0);
                if(IS_INSTANCE(a) || IS_INSTANCE(b))
                {
                    STORE_FRAME;
                    if(!vm_arith_comp(T, MM_EQ, a, b))
                    {
                        DROP(2);
                        PUSH(BOOL_VAL(tea_val_equal(a, b)));
                        DISPATCH();
                    }
                    READ_FRAME();
                    DISPATCH();
                }
                else
                {
                    DROP(2);
                    PUSH(BOOL_VAL(tea_val_equal(a, b)));
                }
                DISPATCH();
            }
            CASE_CODE(BC_GREATER):
            {
                BINARY_OP(BOOL_VAL, (a > b), MM_GT, double);
                DISPATCH();
            }
            CASE_CODE(BC_GREATER_EQUAL):
            {
                BINARY_OP(BOOL_VAL, (a >= b), MM_GE, double);
                DISPATCH();
            }
            CASE_CODE(BC_LESS):
            {
                BINARY_OP(BOOL_VAL, (a < b), MM_LT, double);
                DISPATCH();
            }
            CASE_CODE(BC_LESS_EQUAL):
            {
                BINARY_OP(BOOL_VAL, (a <= b), MM_LE, double);
                DISPATCH();
            }
            CASE_CODE(BC_ADD):
            {
                BINARY_OP(NUMBER_VAL, (a + b), MM_PLUS, double);
                DISPATCH();
            }
            CASE_CODE(BC_SUBTRACT):
            {
                BINARY_OP(NUMBER_VAL, (a - b), MM_MINUS, double);
                DISPATCH();
            }
            CASE_CODE(BC_MULTIPLY):
            {
                BINARY_OP(NUMBER_VAL, (a * b), MM_MULT, double);
                DISPATCH();
            }
            CASE_CODE(BC_DIVIDE):
            {
                BINARY_OP(NUMBER_VAL, (a / b), MM_DIV, double);
                DISPATCH();
            }
            CASE_CODE(BC_MOD):
            {
                BINARY_OP(NUMBER_VAL, (fmod(a, b)), MM_MOD, double);
                DISPATCH();
            }
            CASE_CODE(BC_POW):
            {
                BINARY_OP(NUMBER_VAL, (pow(a, b)), MM_POW, double);
                DISPATCH();
            }
            CASE_CODE(BC_BAND):
            {
                BINARY_OP(NUMBER_VAL, (a & b), MM_BAND, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BOR):
            {
                BINARY_OP(NUMBER_VAL, (a | b), MM_BOR, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BNOT):
            {
                UNARY_OP(NUMBER_VAL, (~v), MM_BNOT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_BXOR):
            {
                BINARY_OP(NUMBER_VAL, (a ^ b), MM_BXOR, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_LSHIFT):
            {
                BINARY_OP(NUMBER_VAL, (a << b), MM_LSHIFT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_RSHIFT):
            {
                BINARY_OP(NUMBER_VAL, (a >> b), MM_RSHIFT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BC_NOT):
            {
                PUSH(BOOL_VAL(tea_obj_isfalse(POP())));
                DISPATCH();
            }
            CASE_CODE(BC_NEGATE):
            {
                UNARY_OP(NUMBER_VAL, (-v), MM_MINUS, double);
                DISPATCH();
            }
            CASE_CODE(BC_MULTI_CASE):
            {
                int count = READ_BYTE();
                Value switch_value = PEEK(count + 1);
                Value case_value = POP();
                for(int i = 0; i < count; i++)
                {
                    if(tea_val_equal(switch_value, case_value))
                    {
                        i++;
                        while(i <= count)
                        {
                            DROP(1);
                            i++;
                        }
                        break;
                    }
                    case_value = POP();
                }
                PUSH(case_value);
                DISPATCH();
            }
            CASE_CODE(BC_COMPARE_JUMP):
            {
                uint16_t offset = READ_SHORT();
                Value a = POP();
                if(!tea_val_equal(PEEK(0), a))
                {
                    ip += offset;
                }
                else
                {
                    DROP(1);
                }
                DISPATCH();
            }
            CASE_CODE(BC_JUMP):
            {
                uint16_t offset = READ_SHORT();
                ip += offset;
                DISPATCH();
            }
            CASE_CODE(BC_JUMP_IF_FALSE):
            {
                uint16_t offset = READ_SHORT();
                if(tea_obj_isfalse(PEEK(0)))
                {
                    ip += offset;
                }
                DISPATCH();
            }
            CASE_CODE(BC_JUMP_IF_NULL):
            {
                uint16_t offset = READ_SHORT();
                if(IS_NULL(PEEK(0)))
                {
                    ip += offset;
                }
                DISPATCH();
            }
            CASE_CODE(BC_LOOP):
            {
                uint16_t offset = READ_SHORT();
                ip -= offset;
                DISPATCH();
            }
            CASE_CODE(BC_CALL):
            {
                int arg_count = READ_BYTE();
                STORE_FRAME;
                if(vm_precall(T, PEEK(arg_count), arg_count))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_INVOKE):
            {
                GCstr* method = READ_STRING();
                int arg_count = READ_BYTE();
                STORE_FRAME;
                if(vm_invoke(T, PEEK(arg_count), method, arg_count))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_SUPER):
            {
                GCstr* method = READ_STRING();
                int arg_count = READ_BYTE();
                GCclass* superclass = AS_CLASS(POP());
                STORE_FRAME;
                if(vm_invoke_from_class(T, superclass, method, arg_count))
                {
                    (T->ci - 1)->state = (CIST_TEA | CIST_CALLING);
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_CLOSURE):
            {
                GCproto* proto = AS_PROTO(READ_CONSTANT());
                GCfuncT* func = tea_func_newT(T, proto);
                PUSH(OBJECT_VAL(func));

                for(int i = 0; i < func->upvalue_count; i++)
                {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if(is_local)
                    {
                        func->upvalues[i] = tea_func_capture(T, base + index);
                    }
                    else
                    {
                        func->upvalues[i] = T->ci->func->upvalues[index];
                    }
                }
                DISPATCH();
            }
            CASE_CODE(BC_CLOSE_UPVALUE):
            {
                tea_func_close(T, T->top - 1);
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_RETURN):
            {
                Value result = POP();
                tea_func_close(T, base);
                STORE_FRAME;
                T->ci--;
                if(!(T->ci->state & CIST_CALLING))
                {
                    T->base = T->ci->base;
                    T->top = base;
                    if(T->ci->cfunc != NULL && T->ci->cfunc->type == C_FUNCTION)
                    {
                        T->base++;
                    }
                    PUSH(result);
                    return;
                }
                T->base = T->ci->base;
                T->top = base;
                PUSH(result);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_GET_ITER):
            {
                uint8_t slot1 = READ_BYTE();    /* seq */
                uint8_t slot2 = READ_BYTE();    /* iter */

                Value seq = base[slot1];
                Value iter = base[slot2];

                PUSH(seq);
                PUSH(iter);

                /* iterate */
                STORE_FRAME;
                if(!vm_iterator_call(T, MM_ITER, seq))
                {
                    RUNTIME_ERROR("'%s' is not iterable", tea_val_type(seq));
                }
                READ_FRAME();

                base[slot2] = PEEK(0);

                iter = base[slot2];
                DISPATCH();
            }
            CASE_CODE(BC_FOR_ITER):
            {
                uint8_t slot1 = READ_BYTE();    /* seq */
                uint8_t slot2 = READ_BYTE();    /* iter */

                Value seq = base[slot1];
                Value iter = base[slot2];

                PUSH(seq);
                PUSH(iter);

                /* iteratorvalue */
                STORE_FRAME;
                if(!vm_iterator_call(T, MM_NEXT, seq))
                {
                    RUNTIME_ERROR("'%s' is not iterable", tea_val_type(seq));
                }
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_CLASS):
            {
                PUSH(OBJECT_VAL(tea_obj_new_class(T, READ_STRING(), NULL)));
                DISPATCH();
            }
            CASE_CODE(BC_SET_CLASS_VAR):
            {
                GCclass* klass = AS_CLASS(PEEK(1));
                GCstr* key = READ_STRING();

                tea_tab_set(T, &klass->statics, key, PEEK(0));
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_INHERIT):
            {
                Value super = PEEK(1);

                if(!IS_CLASS(super))
                {
                    RUNTIME_ERROR("Superclass must be a class");
                }

                GCclass* superclass = AS_CLASS(super);
                if(tea_state_isclass(T, superclass))
                {
                    RUNTIME_ERROR("Cannot inherit from built-in type %s", superclass->name->chars);
                }

                GCclass* klass = AS_CLASS(PEEK(0));
                if(klass == superclass)
                {
                    RUNTIME_ERROR("A class can't inherit from itself");
                }
                klass->super = superclass;
                klass->constructor = superclass->constructor;

                tea_tab_addall(T, &superclass->methods, &klass->methods);
                tea_tab_addall(T, &superclass->statics, &klass->statics);
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_METHOD):
            {
                vm_define_method(T, READ_STRING());
                DISPATCH();
            }
            CASE_CODE(BC_EXTENSION_METHOD):
            {
                if(!IS_CLASS(PEEK(1)))
                {
                    RUNTIME_ERROR("Cannot assign extension method to %s", tea_val_type(PEEK(1)));
                }
                vm_define_method(T, READ_STRING());
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_STRING):
            {
                GCstr* path_name = READ_STRING();
                STORE_FRAME;
                tea_imp_relative(T, T->ci->func->proto->module->path, path_name);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_NAME):
            {
                GCstr* name = READ_STRING();
                STORE_FRAME;
                tea_imp_logical(T, name);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_VARIABLE):
            {
                GCstr* variable = READ_STRING();

                Value module_variable;
                if(!tea_tab_get(&T->last_module->values, variable, &module_variable))
                {
                    RUNTIME_ERROR("%s can't be found in module %s", variable->chars, T->last_module->name->chars);
                }

                PUSH(module_variable);
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_ALIAS):
            {
                PUSH(OBJECT_VAL(T->last_module));
                DISPATCH();
            }
            CASE_CODE(BC_IMPORT_END):
            {
                T->last_module = T->ci->func->proto->module;
                DISPATCH();
            }
            CASE_CODE(BC_END):
            {
                DISPATCH();
            }
        }
    }
}
#undef PUSH
#undef POP
#undef PEEK
#undef DROP
#undef STORE_FRAME
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
#undef BINARY_OP_FUNCTION
#undef RUNTIME_ERROR

static void vm_restore_stack(tea_State* T)
{
    T->stack_max = T->stack + T->stack_size - 1;
    if(T->ci_size > TEA_MAX_CALLS)
    {
        int inuse = (T->ci - T->ci_base);
        if(inuse + 1 < TEA_MAX_CALLS)
        {
            tea_state_reallocci(T, TEA_MAX_CALLS);
        }
    }
}

int tea_vm_pcall(tea_State* T, tea_CPFunction func, void* u, ptrdiff_t old_top)
{
    int oldnccalls = T->nccalls;
    ptrdiff_t old_ci = cisave(T, T->ci);
    int status = tea_err_protected(T, func, u);
    if(status != TEA_OK)    /* An error occurred? */
    {
        Value* old = stackrestore(T, old_top);
        T->open_upvalues = NULL;
        T->nccalls = oldnccalls;
        T->ci = cirestore(T, old_ci);
        T->base = T->ci->base;
        T->top = old;
        tea_vm_push(T, NULL_VAL);
        vm_restore_stack(T);
    }
    return status;
}

void tea_vm_call(tea_State* T, Value func, int arg_count)
{
    if(++T->nccalls >= TEA_MAX_CCALLS)
    {
        tea_err_run(T, "C stack overflow");
    }

    if(vm_precall(T, func, arg_count))
    {
        vm_execute(T);
    }
    T->nccalls--;
}