/*
** tea_vm.c
** Teascript virtual machine
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define tea_vm_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_parser.h"
#include "tea_debug.h"
#include "tea_object.h"
#include "tea_func.h"
#include "tea_map.h"
#include "tea_string.h"
#include "tea_memory.h"
#include "tea_vm.h"
#include "tea_utf.h"
#include "tea_import.h"
#include "tea_do.h"

static void invoke_from_class(TeaState* T, TeaOClass* klass, TeaOString* name, int arg_count)
{
    TeaValue method;
    if(!tea_tab_get(&klass->methods, name, &method))
    {
        tea_vm_error(T, "Undefined method '%s'", name->chars);
    }

    tea_do_precall(T, method, arg_count);
}

static void invoke(TeaState* T, TeaValue receiver, TeaOString* name, int arg_count)
{
    if(!IS_OBJECT(receiver))
    {
        tea_vm_error(T, "Only objects have methods, %s given", tea_val_type(receiver));
    }

    switch(OBJECT_TYPE(receiver))
    {
        case OBJ_MODULE:
        {
            TeaOModule* module = AS_MODULE(receiver);

            TeaValue value;
            if(tea_tab_get(&module->values, name, &value))
            {
                tea_do_precall(T, value, arg_count);
                return;
            }

            tea_vm_error(T, "Undefined variable '%s' in '%s' module", name->chars, module->name->chars);
        }
        case OBJ_INSTANCE:
        {
            TeaOInstance* instance = AS_INSTANCE(receiver);

            TeaValue value;
            if(tea_tab_get(&instance->fields, name, &value))
            {
                T->top[-arg_count - 1] = value;
                tea_do_precall(T, value, arg_count);
                return;
            }

            invoke_from_class(T, instance->klass, name, arg_count);
            return;
        }
        case OBJ_CLASS:
        {
            TeaOClass* klass = AS_CLASS(receiver);
            TeaValue method;
            if(tea_tab_get(&klass->methods, name, &method))
            {
                if(IS_NATIVE(method) || AS_CLOSURE(method)->function->type != TYPE_STATIC)
                {
                    tea_vm_error(T, "'%s' is not static. Only static methods can be invoked directly from a class", name->chars);
                }

                tea_do_precall(T, method, arg_count);
                return;
            }

            tea_vm_error(T, "Undefined method '%s'", name->chars);
        }
        default:
        {
            TeaOClass* type = tea_state_get_class(T, receiver);
            if(type != NULL)
            {
                TeaValue value;
                if(tea_tab_get(&type->methods, name, &value))
                {
                    tea_do_precall(T, value, arg_count);
                    return;
                }

                tea_vm_error(T, "%s has no method %s()", tea_obj_type(receiver), name->chars);
            }
        }
    }
}

static bool bind_method(TeaState* T, TeaOClass* klass, TeaOString* name)
{
    TeaValue method;
    if(!tea_tab_get(&klass->methods, name, &method))
    {
        tea_vm_error(T, "Undefined method '%s'", name->chars);
    }

    TeaOBoundMethod* bound = tea_obj_new_bound_method(T, tea_vm_peek(T, 0), method);
    tea_vm_pop(T, 1);
    tea_vm_push(T, OBJECT_VAL(bound));
    return true;
}

static void in_(TeaState* T, TeaValue object, TeaValue value)
{
    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_STRING:
            {
                if(!IS_STRING(value))
                {
                    tea_vm_pop(T, 2);
                    tea_vm_push(T, FALSE_VAL);
                    return;
                }

                TeaOString* string = AS_STRING(object);
                TeaOString* sub = AS_STRING(value);

                if(sub == string)
                {
                    tea_vm_pop(T, 2);
                    tea_vm_push(T, TRUE_VAL);
                    return;
                }

                tea_vm_pop(T, 2);
                tea_vm_push(T, BOOL_VAL(strstr(string->chars, sub->chars) != NULL));
                return;
            }
            case OBJ_RANGE:
            {
                if(!IS_NUMBER(value))
                {
                    tea_vm_push(T, FALSE_VAL);
                    return;
                }

                double number = AS_NUMBER(value);
                TeaORange* range = AS_RANGE(object);
                int start = range->start;
                int end = range->end;

                if(number < start || number > end)
                {
                    tea_vm_pop(T, 2);
                    tea_vm_push(T, FALSE_VAL);
                    return;
                }

                tea_vm_pop(T, 2);
                tea_vm_push(T, TRUE_VAL);
                return;
            }
            case OBJ_LIST:
            {
                TeaOList* list = AS_LIST(object);

                for(int i = 0; i < list->items.count; i++)
                {
                    if(tea_val_equal(list->items.values[i], value))
                    {
                        tea_vm_pop(T, 2);
                        tea_vm_push(T, TRUE_VAL);
                        return;
                    }
                }

                tea_vm_pop(T, 2);
                tea_vm_push(T, FALSE_VAL);
                return;
            }
            case OBJ_MAP:
            {
                TeaOMap* map = AS_MAP(object);
                TeaValue _;

                tea_vm_pop(T, 2);
                tea_vm_push(T, BOOL_VAL(tea_map_get(map, value, &_)));
                return;
            }
            default:
                break;
        }
    }

    tea_vm_error(T, "%s is not an iterable", tea_val_type(object));
}

static bool slice(TeaState* T, TeaValue object, TeaValue start_index, TeaValue end_index, TeaValue step_index)
{
    if(!IS_NUMBER(step_index) || (!IS_NUMBER(end_index) && !IS_NULL(end_index)) || !IS_NUMBER(step_index))
    {
        tea_vm_error(T, "Slice index must be a number");
        return false;
    }

    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_LIST:
            {
                TeaOList* new_list = tea_obj_new_list(T);
                tea_vm_push(T, OBJECT_VAL(new_list));
                TeaOList* list = AS_LIST(object);

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

                tea_vm_pop(T, 5);   /* +1 for the pushed list */
                tea_vm_push(T, OBJECT_VAL(new_list));
                return true;
            }
            case OBJ_STRING:
            {
                TeaOString* string = AS_STRING(object);
                int length = tea_utf_length(string);

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
                    tea_vm_pop(T, 4);
                    tea_vm_push(T, OBJECT_VAL(tea_str_literal(T, "")));
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

    tea_vm_error(T, "%s is not sliceable", tea_val_type(object));
    return false;
}

static void subscript(TeaState* T, TeaValue index_value, TeaValue subscript_value)
{
    if(!IS_OBJECT(subscript_value))
    {
        tea_vm_error(T, "%s is not subscriptable", tea_val_type(subscript_value));
    }

    switch(OBJECT_TYPE(subscript_value))
    {
        case OBJ_INSTANCE:
        {
            TeaOInstance* instance = AS_INSTANCE(subscript_value);

            TeaOString* subs = T->opm_name[MT_SUBSCRIPT];

            TeaValue method;
            if(tea_tab_get(&instance->klass->methods, subs, &method))
            {
                tea_vm_push(T, NULL_VAL);
                tea_do_precall(T, method, 2);
                return;
            }

            tea_vm_error(T, "'%s' instance is not subscriptable", instance->klass->name);
        }
        case OBJ_RANGE:
        {
            if(!IS_NUMBER(index_value))
            {
                tea_vm_error(T, "Range index must be a number");
            }

            TeaORange* range = AS_RANGE(subscript_value);
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

            tea_vm_error(T, "Range index out of bounds");
        }
        case OBJ_LIST:
        {
            if(!IS_NUMBER(index_value))
            {
                tea_vm_error(T, "List index must be a number");
            }

            TeaOList* list = AS_LIST(subscript_value);
            int index = AS_NUMBER(index_value);

            /* Allow negative indexes */
            if(index < 0)
            {
                index = list->items.count + index;
            }

            if(index >= 0 && index < list->items.count)
            {
                tea_vm_pop(T, 2);
                tea_vm_push(T, list->items.values[index]);
                return;
            }

            tea_vm_error(T, "List index out of bounds");
        }
        case OBJ_MAP:
        {
            TeaOMap* map = AS_MAP(subscript_value);
            if(!tea_map_validkey(index_value))
            {
                tea_vm_error(T, "Map key isn't hashable");
            }

            TeaValue value;
            tea_vm_pop(T, 2);
            if(tea_map_get(map, index_value, &value))
            {
                tea_vm_push(T, value);
                return;
            }

            tea_vm_error(T, "Key does not exist within map");
        }
        case OBJ_STRING:
        {
            if(!IS_NUMBER(index_value))
            {
                tea_vm_error(T, "String index must be a number (got %s)", tea_val_type(index_value));
            }

            TeaOString* string = AS_STRING(subscript_value);
            int index = AS_NUMBER(index_value);
            int real_length = tea_utf_length(string);

            /* Allow negative indexes */
            if(index < 0)
            {
                index = real_length + index;
            }

            if(index >= 0 && index < string->length)
            {
                tea_vm_pop(T, 2);
                TeaOString* c = tea_utf_codepoint_at(T, string, tea_utf_char_offset(string->chars, index));
                tea_vm_push(T, OBJECT_VAL(c));
                return;
            }

            tea_vm_error(T, "String index out of bounds");
        }
        default:
            break;
    }

    tea_vm_error(T, "'%s' is not subscriptable", tea_val_type(subscript_value));
}

static void subscript_store(TeaState* T, TeaValue item_value, TeaValue index_value, TeaValue subscript_value, bool assign)
{
    if(!IS_OBJECT(subscript_value))
    {
        tea_vm_error(T, "'%s' is not subscriptable", tea_val_type(subscript_value));
    }

    switch(OBJECT_TYPE(subscript_value))
    {
        case OBJ_INSTANCE:
        {
            TeaOInstance* instance = AS_INSTANCE(subscript_value);

            TeaOString* subs = T->opm_name[MT_SUBSCRIPT];

            TeaValue method;
            if(tea_tab_get(&instance->klass->methods, subs, &method))
            {
                tea_do_precall(T, method, 2);
                return;
            }

            tea_vm_error(T, "'%s' does not support item assignment", instance->klass->name);
        }
        case OBJ_LIST:
        {
            if(!IS_NUMBER(index_value))
            {
                tea_vm_error(T, "List index must be a number (got %s)", tea_val_type(index_value));
            }

            TeaOList* list = AS_LIST(subscript_value);
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
                    tea_vm_pop(T, 3);
                    tea_vm_push(T, item_value);
                }
                else
                {
                    T->top[-1] = list->items.values[index];
                    tea_vm_push(T, item_value);
                }
                return;
            }

            tea_vm_error(T, "List index out of bounds");
        }
        case OBJ_MAP:
        {
            TeaOMap* map = AS_MAP(subscript_value);
            if(!tea_map_validkey(index_value))
            {
                tea_vm_error(T, "Map key isn't hashable");
            }

            if(assign)
            {
                tea_map_set(T, map, index_value, item_value);
                tea_vm_pop(T, 3);
                tea_vm_push(T, item_value);
            }
            else
            {
                TeaValue map_value;
                if(!tea_map_get(map, index_value, &map_value))
                {
                    tea_vm_error(T, "Key does not exist within the map");
                }
                T->top[-1] = map_value;
                tea_vm_push(T, item_value);
            }
            return;
        }
        default:
            break;
    }

    tea_vm_error(T, "'%s' does not support item assignment", tea_val_type(subscript_value));
}

static void get_property(TeaState* T, TeaValue receiver, TeaOString* name, bool dopop)
{
    if(!IS_OBJECT(receiver))
    {
        tea_vm_error(T, "Only objects have properties");
    }

    switch(OBJECT_TYPE(receiver))
    {
        case OBJ_INSTANCE:
        {
            TeaOInstance* instance = AS_INSTANCE(receiver);

            TeaValue value;
            if(tea_tab_get(&instance->fields, name, &value))
            {
                if(dopop)
                {
                    tea_vm_pop(T, 1); /* Instance */
                }
                tea_vm_push(T, value);
                return;
            }

            if(bind_method(T, instance->klass, name))
                return;

            TeaOClass* klass = instance->klass;
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

            tea_vm_error(T, "'%s' instance has no property: '%s'", instance->klass->name->chars, name->chars);
        }
        case OBJ_CLASS:
        {
            TeaOClass* klass = AS_CLASS(receiver);
            TeaOClass* klass_store = klass;

            while(klass != NULL)
            {
                TeaValue value;
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

            tea_vm_error(T, "'%s' class has no property: '%s'.", klass_store->name->chars, name->chars);
        }
        case OBJ_MODULE:
        {
            TeaOModule* module = AS_MODULE(receiver);

            TeaValue value;
            if(tea_tab_get(&module->values, name, &value))
            {
                if(dopop)
                {
                    tea_vm_pop(T, 1); /* Module */
                }
                tea_vm_push(T, value);
                return;
            }

            tea_vm_error(T, "'%s' module has no property: '%s'", module->name->chars, name->chars);
        }
        case OBJ_MAP:
        {
            TeaOMap* map = AS_MAP(receiver);

            TeaValue value;
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

            tea_vm_error(T, "Map has no property: '%s'", name->chars);
        }
        default:
        retry:
        {
            TeaOClass* klass = tea_state_get_class(T, receiver);
            if(klass != NULL)
            {
                TeaValue value;
                if(tea_tab_get(&klass->methods, name, &value))
                {
                    if(IS_NATIVE(value) && AS_NATIVE(value)->type == NATIVE_PROPERTY)
                    {
                        tea_do_precall(T, value, 0);
                    }
                    else
                    {
                        bind_method(T, klass, name);
                    }
                    return;
                }
            }
            break;
        }
    }

    tea_vm_error(T, "'%s' has no property '%s'", tea_obj_type(receiver), name->chars);
}

static void set_property(TeaState* T, TeaOString* name, TeaValue receiver, TeaValue item)
{
    if(IS_OBJECT(receiver))
    {
        switch(OBJECT_TYPE(receiver))
        {
            case OBJ_INSTANCE:
            {
                TeaOInstance* instance = AS_INSTANCE(receiver);
                tea_tab_set(T, &instance->fields, name, item);
                tea_vm_pop(T, 2);
                tea_vm_push(T, item);
                return;
            }
            case OBJ_CLASS:
            {
                TeaOClass* klass = AS_CLASS(receiver);
                tea_tab_set(T, &klass->statics, name, item);
                tea_vm_pop(T, 2);
                tea_vm_push(T, item);
                return;
            }
            case OBJ_MAP:
            {
                TeaOMap* map = AS_MAP(receiver);
                tea_map_set(T, map, OBJECT_VAL(name), item);
                tea_vm_pop(T, 2);
                tea_vm_push(T, item);
                return;
            }
            case OBJ_MODULE:
            {
                TeaOModule* module = AS_MODULE(receiver);
                tea_tab_set(T, &module->values, name, item);
                tea_vm_pop(T, 2);
                tea_vm_push(T, item);
                return;
            }
            default:
                break;
        }
    }

    tea_vm_error(T, "Cannot set property on type '%s'", tea_val_type(receiver));
}

static void define_method(TeaState* T, TeaOString* name)
{
    TeaValue method = tea_vm_peek(T, 0);
    TeaOClass* klass = AS_CLASS(tea_vm_peek(T, 1));
    tea_tab_set(T, &klass->methods, name, method);
    if(name == T->constructor_string) klass->constructor = method;
    tea_vm_pop(T, 1);
}

static void concatenate(TeaState* T)
{
    TeaOString* b = AS_STRING(tea_vm_peek(T, 0));
    TeaOString* a = AS_STRING(tea_vm_peek(T, 1));

    int length = a->length + b->length;
    char* chars = TEA_ALLOCATE(T, char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    TeaOString* result = tea_str_take(T, chars, length);
    tea_vm_pop(T, 2);
    tea_vm_push(T, OBJECT_VAL(result));
}

static void repeat(TeaState* T)
{
    TeaOString* string;
    int n;

    if(IS_STRING(tea_vm_peek(T, 0)) && IS_NUMBER(tea_vm_peek(T, 1)))
    {
        string = AS_STRING(tea_vm_peek(T, 0));
        n = AS_NUMBER(tea_vm_peek(T, 1));
    }
    else if(IS_NUMBER(tea_vm_peek(T, 0)) && IS_STRING(tea_vm_peek(T, 1)))
    {
        n = AS_NUMBER(tea_vm_peek(T, 0));
        string = AS_STRING(tea_vm_peek(T, 1));
    }

    if(n <= 0)
    {
        TeaOString* s = tea_str_literal(T, "");
        tea_vm_pop(T, 2);
        tea_vm_push(T, OBJECT_VAL(s));
        return;
    }
    else if(n == 1)
    {
        tea_vm_pop(T, 2);
        tea_vm_push(T, OBJECT_VAL(string));
        return;
    }

    int length = string->length;
    char* chars = TEA_ALLOCATE(T, char, (n * length) + 1);

    int i;
    char* p;
    for(i = 0, p = chars; i < n; ++i, p += length)
    {
        memcpy(p, string->chars, length);
    }
    *p = '\0';

    TeaOString* result = tea_str_take(T, chars, strlen(chars));
    tea_vm_pop(T, 2);
    tea_vm_push(T, OBJECT_VAL(result));
}

static bool get_op_method(TeaOString* key, TeaValue v, TeaValue* method)
{
    if(!IS_INSTANCE(v))
        return false;

    TeaOInstance* instance = AS_INSTANCE(v);
    return tea_tab_get(&instance->klass->methods, key, method);
}

static void unary_arith(TeaState* T, TeaOpMethod op, TeaValue v)
{
    TeaOString* method_name = T->opm_name[op];

    TeaValue method;
    if(!get_op_method(method_name, v, &method))
    {
        tea_vm_error(T, "Attempt to use '%s' unary operator with %s", method_name->chars, tea_val_type(v));
    }

    tea_vm_pop(T, 1);
    tea_vm_push(T, method);
    tea_vm_push(T, v);
    tea_vm_push(T, NULL_VAL);
    tea_do_precall(T, method, 2);
}

static void arith(TeaState* T, TeaOpMethod op, TeaValue a, TeaValue b)
{
    TeaOString* method_name = T->opm_name[op];

    TeaValue method;
    bool found = get_op_method(method_name, a, &method);    /* try first operand */
    if(!found)
    {
        found = get_op_method(method_name, b, &method); /* try second operand */
    }
    if(!found)
    {
        tea_vm_error(T, "Attempt to use '%s' operator with %s and %s", method_name->chars, tea_val_type(a), tea_val_type(b));
    }

    tea_vm_pop(T, 2);
    tea_vm_push(T, method);
    tea_vm_push(T, a);
    tea_vm_push(T, b);
    tea_do_precall(T, method, 2);
}

static bool arith_comp(TeaState* T, TeaOpMethod op, TeaValue a, TeaValue b)
{
    TeaOString* method_name = T->opm_name[op];

    TeaValue method;
    bool found = get_op_method(method_name, a, &method);    /* try first operand */
    if(!found)
    {
        found = get_op_method(method_name, b, &method); /* try second operand */
    }
    if(!found)
    {
        return false;
    }

    tea_vm_pop(T, 2);
    tea_vm_push(T, method);
    tea_vm_push(T, a);
    tea_vm_push(T, b);
    tea_do_precall(T, method, 2);
    return true;
}

void tea_vm_error(TeaState* T, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for(TeaCallInfo* ci = T->ci - 1; ci >= T->base_ci; ci--)
    {
        /* Skip stack trace for C functions */
        if(ci->closure == NULL) continue;

        TeaOFunction* function = ci->closure->function;
        size_t instruction = ci->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ", tea_chunk_getline(&function->chunk, instruction));
        if(function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    tea_do_throw(T, TEA_RUNTIME_ERROR);
}

void tea_vm_run(TeaState* T)
{
    register TeaCallInfo* ci;
    register uint8_t* ip;

    register TeaValue* base;

#define PUSH(value) (tea_vm_push(T, value))
#define POP() (tea_vm_pop(T, 1))
#define PEEK(distance) (tea_vm_peek(T, distance))
#define DROP(amount) (T->top -= (amount))
#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

#define STORE_FRAME (ci->ip = ip)
#define READ_FRAME() \
    do \
    { \
        ci = T->ci - 1; \
	    ip = ci->ip; \
	    base = ci->base; \
    } \
    while(false) \

#define READ_CONSTANT() (ci->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define RUNTIME_ERROR(...) \
    do \
    { \
        STORE_FRAME; \
        tea_vm_error(T, __VA_ARGS__); \
        READ_FRAME(); \
        DISPATCH(); \
    } \
    while(false)

#define BINARY_OP(value_type, expr, op_method, type) \
    do \
    { \
        if(IS_NUMBER(PEEK(0)) && IS_NUMBER(PEEK(1))) \
        { \
            type b = AS_NUMBER(POP()); \
            type a = AS_NUMBER(PEEK(0)); \
            T->top[-1] = value_type(expr); \
        } \
        else \
        { \
            STORE_FRAME; \
            arith(T, op_method, PEEK(1), PEEK(0)); \
            READ_FRAME(); \
        } \
    } \
    while(false)

#define UNARY_OP(value_type, expr, op_method, type) \
    do \
    { \
        if(IS_NUMBER(PEEK(0))) \
        { \
            type v = AS_NUMBER(PEEK(0)); \
            T->top[-1] = value_type(expr); \
        } \
        else \
        { \
            STORE_FRAME; \
            unary_arith(T, op_method, PEEK(0)); \
            READ_FRAME(); \
        } \
    } \
    while(false)

#ifdef TEA_DEBUG_TRACE_EXECUTION
    #define TRACE_INSTRUCTIONS() \
        do \
        { \
            TeaChunk* current_chunk = &ci->closure->function->chunk; \
            tea_debug_stack(T); \
            tea_debug_instruction(T, current_chunk, (int)(ip - current_chunk->code)); \
        } \
        while(false)
#else
    #define TRACE_INSTRUCTIONS() do { } while(false)
#endif

#ifdef TEA_COMPUTED_GOTO
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

    #define INTERPRET_LOOP  DISPATCH();
    #define CASE_CODE(name) OP_##name
#else
    #define INTERPRET_LOOP \
        loop: \
            switch(instruction = READ_BYTE())

    #define DISPATCH() goto loop

    #define CASE_CODE(name) case OP_##name
#endif

    READ_FRAME();

    while(true)
    {
        uint8_t instruction;
        INTERPRET_LOOP
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
            CASE_CODE(POP):
            {
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(POP_REPL):
            {
                TeaValue value = PEEK(0);
                if(!IS_NULL(value))
                {
                    tea_tab_set(T, &T->globals, T->repl_string, value);
                    TeaOString* string = tea_val_tostring(T, value);
                    PUSH(OBJECT_VAL(string));
                    fwrite(string->chars, sizeof(char), string->length, stdout);
                    putchar('\n');
                    DROP(1);
                }
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(GET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                PUSH(base[slot]);
                DISPATCH();
            }
            CASE_CODE(SET_LOCAL):
            {
                uint8_t slot = READ_BYTE();
                base[slot] = PEEK(0);
                DISPATCH();
            }
            CASE_CODE(GET_GLOBAL):
            {
                TeaOString* name = READ_STRING();
                TeaValue value;
                if(!tea_tab_get(&T->globals, name, &value))
                {
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                PUSH(value);
                DISPATCH();
            }
            CASE_CODE(SET_GLOBAL):
            {
                TeaOString* name = READ_STRING();
                if(tea_tab_set(T, &T->globals, name, PEEK(0)))
                {
                    tea_tab_delete(&T->globals, name);
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                DISPATCH();
            }
            CASE_CODE(GET_MODULE):
            {
                TeaOString* name = READ_STRING();
                TeaValue value;
                if(!tea_tab_get(&ci->closure->function->module->values, name, &value))
                {
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                PUSH(value);
                DISPATCH();
            }
            CASE_CODE(SET_MODULE):
            {
                TeaOString* name = READ_STRING();
                if(tea_tab_set(T, &ci->closure->function->module->values, name, PEEK(0)))
                {
                    tea_tab_delete(&ci->closure->function->module->values, name);
                    RUNTIME_ERROR("Undefined variable '%s'", name->chars);
                }
                DISPATCH();
            }
            CASE_CODE(DEFINE_OPTIONAL):
            {
                int arity = READ_BYTE();
                int arity_optional = READ_BYTE();
                int arg_count = T->top - base - arity_optional - 1;

                /* Temp array while we shuffle the stack
                ** Cannot have more than 255 args to a function, so
                ** we can define this with a constant limit
                */
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

                /* Calculate how many "default" values are required */
                int remaining = arity + arity_optional - arg_count;

                /* Push any "default" values back onto the stack */
                for(int i = remaining; i > 0; i--)
                {
                    PUSH(values[i - 1]);
                }
                DISPATCH();
            }
            CASE_CODE(DEFINE_GLOBAL):
            {
                TeaOString* name = READ_STRING();
                tea_tab_set(T, &T->globals, name, PEEK(0));
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(DEFINE_MODULE):
            {
                TeaOString* name = READ_STRING();
                tea_tab_set(T, &ci->closure->function->module->values, name, PEEK(0));
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(GET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                PUSH(*ci->closure->upvalues[slot]->location);
                DISPATCH();
            }
            CASE_CODE(SET_UPVALUE):
            {
                uint8_t slot = READ_BYTE();
                *ci->closure->upvalues[slot]->location = PEEK(0);
                DISPATCH();
            }
            CASE_CODE(GET_PROPERTY):
            {
                TeaValue receiver = PEEK(0);
                TeaOString* name = READ_STRING();
                STORE_FRAME;
                get_property(T, receiver, name, true);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(GET_PROPERTY_NO_POP):
            {
                TeaValue receiver = PEEK(0);
                TeaOString* name = READ_STRING();
                STORE_FRAME;
                get_property(T, receiver, name, false);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(SET_PROPERTY):
            {
                TeaOString* name = READ_STRING();
                TeaValue receiver = PEEK(1);
                TeaValue item = PEEK(0);
                STORE_FRAME;
                set_property(T, name, receiver, item);
                DISPATCH();
            }
            CASE_CODE(GET_SUPER):
            {
                TeaOString* name = READ_STRING();
                TeaOClass* superclass = AS_CLASS(POP());
                STORE_FRAME;
                bind_method(T, superclass, name);
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

                PUSH(OBJECT_VAL(tea_obj_new_range(T, AS_NUMBER(a), AS_NUMBER(b), AS_NUMBER(c))));
                DISPATCH();
            }
            CASE_CODE(LIST):
            {
                uint8_t item_count = READ_BYTE();
                TeaOList* list = tea_obj_new_list(T);

                PUSH(OBJECT_VAL(list));

                /* Add items to list */
                for(int i = item_count; i > 0; i--)
                {
                    if(IS_RANGE(PEEK(i)))
                    {
                        TeaORange* range = AS_RANGE(PEEK(i));

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

                /* Pop items from stack */
                T->top -= item_count + 1;

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

                TeaOList* list = AS_LIST(POP());

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

                TeaOList* list = AS_LIST(POP());

                if(var_count > list->items.count)
                {
                    RUNTIME_ERROR("Not enough values to unpack");
                }

                for(int i = 0; i < list->items.count; i++)
                {
                    if(i == rest_pos)
                    {
                        TeaOList* rest_list = tea_obj_new_list(T);
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
                TeaOMap* map = tea_map_new(T);

                PUSH(OBJECT_VAL(map));

                for(int i = item_count * 2; i > 0; i -= 2)
                {
                    if(!tea_map_validkey(PEEK(i)))
                    {
                        RUNTIME_ERROR("Map key isn't hashable");
                    }

                    tea_map_set(T, map, PEEK(i), PEEK(i - 1));
                }

                T->top -= item_count * 2 + 1;

                PUSH(OBJECT_VAL(map));
                DISPATCH();
            }
            CASE_CODE(SUBSCRIPT):
            {
                TeaValue index = PEEK(0);
                TeaValue list = PEEK(1);
                STORE_FRAME;
                subscript(T, index, list);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(SUBSCRIPT_STORE):
            {
                TeaValue item = PEEK(0);
                TeaValue index = PEEK(1);
                TeaValue list = PEEK(2);
                STORE_FRAME;
                subscript_store(T, item, index, list, true);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(SUBSCRIPT_PUSH):
            {
                TeaValue item = PEEK(0);
                TeaValue index = PEEK(1);
                TeaValue list = PEEK(2);
                STORE_FRAME;
                subscript_store(T, item, index, list, false);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(SLICE):
            {
                TeaValue step = PEEK(0);
                TeaValue end = PEEK(1);
                TeaValue start = PEEK(2);
                TeaValue object = PEEK(3);
                STORE_FRAME;
                slice(T, object, start, end, step);
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
                    DROP(2); /* Drop the instance and class */
                    PUSH(FALSE_VAL);
                    DISPATCH();
                }

                TeaOClass* instance_klass = AS_INSTANCE(instance)->klass;
                TeaOClass* type = AS_CLASS(klass);
                bool found = false;

                while(instance_klass != NULL)
                {
                    if(instance_klass == type)
                    {
                        found = true;
                        break;
                    }

                    instance_klass = (TeaOClass*)instance_klass->super;
                }

                DROP(2); /* Drop the instance and class */
                PUSH(BOOL_VAL(found));

                DISPATCH();
            }
            CASE_CODE(IN):
            {
                TeaValue object = PEEK(0);
                TeaValue value = PEEK(1);
                STORE_FRAME;
                in_(T, object, value);
                DISPATCH();
            }
            CASE_CODE(EQUAL):
            {
                TeaValue a = PEEK(1);
                TeaValue b = PEEK(0);
                if(IS_INSTANCE(a) || IS_INSTANCE(b))
                {
                    STORE_FRAME;
                    if(!arith_comp(T, MT_EQ, a, b))
                    {
                        DROP(2);
                        PUSH(BOOL_VAL(tea_val_equal(a, b)));
                        DISPATCH();
                    }
                    READ_FRAME();
                    DISPATCH();
                }
                DROP(2);
                PUSH(BOOL_VAL(tea_val_equal(a, b)));
                DISPATCH();
            }
            CASE_CODE(GREATER):
            {
                BINARY_OP(BOOL_VAL, (a > b), MT_GT, double);
                DISPATCH();
            }
            CASE_CODE(GREATER_EQUAL):
            {
                BINARY_OP(BOOL_VAL, (a >= b), MT_GE, double);
                DISPATCH();
            }
            CASE_CODE(LESS):
            {
                BINARY_OP(BOOL_VAL, (a < b), MT_LT, double);
                DISPATCH();
            }
            CASE_CODE(LESS_EQUAL):
            {
                BINARY_OP(BOOL_VAL, (a <= b), MT_LE, double);
                DISPATCH();
            }
            CASE_CODE(ADD):
            {
                TeaValue a = PEEK(1);
                TeaValue b = PEEK(0);
                if(IS_NUMBER(a) && IS_NUMBER(b))
                {
                    double b = AS_NUMBER(POP());
                    double a = AS_NUMBER(PEEK(0));
                    T->top[-1] = NUMBER_VAL(a + b);
                }
                else if(IS_STRING(a) && IS_STRING(b))
                {
                    concatenate(T);
                }
                else if(IS_LIST(a) && IS_LIST(b))
                {
                    TeaOList* l1 = AS_LIST(a);
                    TeaOList* l2 = AS_LIST(b);

                    for(int i = 0; i < l2->items.count; i++)
                    {
                        tea_write_value_array(T, &l1->items, l2->items.values[i]);
                    }

                    DROP(2);
                    PUSH(OBJECT_VAL(l1));
                }
                else if(IS_MAP(a) && IS_MAP(b))
                {
                    TeaOMap* m1 = AS_MAP(a);
                    TeaOMap* m2 = AS_MAP(b);

                    tea_map_add_all(T, m2, m1);

                    DROP(2);
                    PUSH(OBJECT_VAL(m1));
                }
                else
                {
                    STORE_FRAME;
                    arith(T, MT_PLUS, a, b);
                    READ_FRAME();
                }
                DISPATCH();
            }
            CASE_CODE(SUBTRACT):
            {
                BINARY_OP(NUMBER_VAL, (a - b), MT_MINUS, double);
                DISPATCH();
            }
            CASE_CODE(MULTIPLY):
            {
                TeaValue a = PEEK(1);
                TeaValue b = PEEK(0);
                if(IS_NUMBER(a) && IS_NUMBER(b))
                {
                    double b = AS_NUMBER(POP());
                    double a = AS_NUMBER(PEEK(0));
                    T->top[-1] = NUMBER_VAL(a * b);
                }
                else if((IS_STRING(a) && IS_NUMBER(b)) || (IS_NUMBER(a) && IS_STRING(b)))
                {
                    repeat(T);
                }
                else
                {
                    STORE_FRAME;
                    arith(T, MT_MULT, a, b);
                    READ_FRAME();
                }
                DISPATCH();
            }
            CASE_CODE(DIVIDE):
            {
                BINARY_OP(NUMBER_VAL, (a / b), MT_DIV, double);
                DISPATCH();
            }
            CASE_CODE(MOD):
            {
                BINARY_OP(NUMBER_VAL, (fmod(a, b)), MT_MOD, double);
                DISPATCH();
            }
            CASE_CODE(POW):
            {
                BINARY_OP(NUMBER_VAL, (pow(a, b)), MT_POW, double);
                DISPATCH();
            }
            CASE_CODE(BAND):
            {
                BINARY_OP(NUMBER_VAL, (a & b), MT_BAND, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BOR):
            {
                BINARY_OP(NUMBER_VAL, (a | b), MT_BOR, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BNOT):
            {
                UNARY_OP(NUMBER_VAL, (~v), MT_BNOT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(BXOR):
            {
                BINARY_OP(NUMBER_VAL, (a ^ b), MT_BXOR, uint32_t);
                DISPATCH();
            }
            CASE_CODE(LSHIFT):
            {
                BINARY_OP(NUMBER_VAL, (a << b), MT_LSHIFT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(RSHIFT):
            {
                BINARY_OP(NUMBER_VAL, (a >> b), MT_RSHIFT, uint32_t);
                DISPATCH();
            }
            CASE_CODE(AND):
            {
                uint16_t offset = READ_SHORT();

                if(tea_obj_isfalse(PEEK(0)))
                {
                    ip += offset;
                }
                else
                {
                    DROP(1);
                }

                DISPATCH();
            }
            CASE_CODE(OR):
            {
                uint16_t offset = READ_SHORT();

                if(tea_obj_isfalse(PEEK(0)))
                {
                    DROP(1);
                }
                else
                {
                    ip += offset;
                }

                DISPATCH();
            }
            CASE_CODE(NOT):
            {
                PUSH(BOOL_VAL(tea_obj_isfalse(POP())));
                DISPATCH();
            }
            CASE_CODE(NEGATE):
            {
                UNARY_OP(NUMBER_VAL, (-v), MT_MINUS, double);
                DISPATCH();
            }
            CASE_CODE(MULTI_CASE):
            {
                int count = READ_BYTE();
                TeaValue switch_value = PEEK(count + 1);
                TeaValue case_value = POP();
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
            CASE_CODE(COMPARE_JUMP):
            {
                uint16_t offset = READ_SHORT();
                TeaValue a = POP();
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
            CASE_CODE(JUMP):
            {
                uint16_t offset = READ_SHORT();
                ip += offset;
                DISPATCH();
            }
            CASE_CODE(JUMP_IF_FALSE):
            {
                uint16_t offset = READ_SHORT();
                if(tea_obj_isfalse(PEEK(0)))
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
                tea_do_precall(T, PEEK(arg_count), arg_count);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(INVOKE):
            {
                TeaOString* method = READ_STRING();
                int arg_count = READ_BYTE();
                STORE_FRAME;
                invoke(T, PEEK(arg_count), method, arg_count);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(SUPER):
            {
                TeaOString* method = READ_STRING();
                int arg_count = READ_BYTE();
                TeaOClass* superclass = AS_CLASS(POP());
                STORE_FRAME;
                invoke_from_class(T, superclass, method, arg_count);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(CLOSURE):
            {
                TeaOFunction* function = AS_FUNCTION(READ_CONSTANT());
                TeaOClosure* closure = tea_func_new_closure(T, function);
                PUSH(OBJECT_VAL(closure));

                for(int i = 0; i < closure->upvalue_count; i++)
                {
                    uint8_t is_local = READ_BYTE();
                    uint8_t index = READ_BYTE();
                    if(is_local)
                    {
                        closure->upvalues[i] = tea_func_capture(T, base + index);
                    }
                    else
                    {
                        closure->upvalues[i] = ci->closure->upvalues[index];
                    }
                }
                DISPATCH();
            }
            CASE_CODE(CLOSE_UPVALUE):
            {
                tea_func_close(T, T->top - 1);
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(RETURN):
            {
                TeaValue result = POP();
                tea_func_close(T, base);
                STORE_FRAME;
                T->ci--;
                if(T->ci == T->base_ci)
                {
                    T->base = base;
                    T->top = base;
                    return;
                }

                TeaCallInfo* last = T->ci - 1;
                if(last->closure == NULL)
                {
                    T->base = last->base;
                    T->top = base;
                    PUSH(result);
                    return;
                }
                T->base = base;
                T->top = base;
                PUSH(result);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(CLASS):
            {
                PUSH(OBJECT_VAL(tea_obj_new_class(T, READ_STRING(), NULL)));
                DISPATCH();
            }
            CASE_CODE(SET_CLASS_VAR):
            {
                TeaOClass* klass = AS_CLASS(PEEK(1));
                TeaOString* key = READ_STRING();

                tea_tab_set(T, &klass->statics, key, PEEK(0));
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(INHERIT):
            {
                TeaValue super = PEEK(1);

                if(!IS_CLASS(super))
                {
                    RUNTIME_ERROR("Superclass must be a class");
                }

                TeaOClass* superclass = AS_CLASS(super);
                if(tea_state_isclass(T, superclass))
                {
                    RUNTIME_ERROR("Cannot inherit from built-in type %s", superclass->name->chars);
                }

                TeaOClass* klass = AS_CLASS(PEEK(0));
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
            CASE_CODE(METHOD):
            {
                define_method(T, READ_STRING());
                DISPATCH();
            }
            CASE_CODE(EXTENSION_METHOD):
            {
                if(!IS_CLASS(PEEK(1)))
                {
                    RUNTIME_ERROR("Cannot assign extension method to %s", tea_val_type(PEEK(1)));
                }
                define_method(T, READ_STRING());
                DROP(1);
                DISPATCH();
            }
            CASE_CODE(IMPORT_STRING):
            {
                TeaOString* path_name = READ_STRING();
                STORE_FRAME;
                tea_imp_relative(T, ci->closure->function->module->path, path_name);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(IMPORT_NAME):
            {
                TeaOString* name = READ_STRING();
                STORE_FRAME;
                tea_imp_logical(T, name);
                READ_FRAME();
                DISPATCH();
            }
            CASE_CODE(IMPORT_VARIABLE):
            {
                TeaOString* variable = READ_STRING();

                TeaValue module_variable;
                if(!tea_tab_get(&T->last_module->values, variable, &module_variable))
                {
                    RUNTIME_ERROR("%s can't be found in module %s", variable->chars, T->last_module->name->chars);
                }

                PUSH(module_variable);
                DISPATCH();
            }
            CASE_CODE(IMPORT_ALIAS):
            {
                PUSH(OBJECT_VAL(T->last_module));
                DISPATCH();
            }
            CASE_CODE(IMPORT_END):
            {
                T->last_module = ci->closure->function->module;
                DISPATCH();
            }
            CASE_CODE(END):
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