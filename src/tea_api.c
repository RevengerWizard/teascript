/*
** tea_api.c
** C API functions for Teascript
*/

#define tea_api_c
#define TEA_CORE

#include "tea.h"

#include "tea_state.h"
#include "tea_string.h"
#include "tea_map.h"
#include "tea_vm.h"
#include "tea_do.h"

static TeaValue index2value(TeaState* T, int index)
{
    if(index >= 0)
    {
        return T->base[index];
    }
    else
    {
        return T->top[index];
    }
}

static TeaValue* index2stack(TeaState* T, int index)
{
    if(index >= 0)
    {
        TeaValue* value = T->base + index;
        if(value >= T->top)
            return NULL;
        else
            return value;
    }
    else
    {
        return T->top + index;
    }
}

TEA_API void tea_set_repl(TeaState* T, int b)
{
    T->repl = b;
}

TEA_API void tea_set_argv(TeaState* T, int argc, const char** argv)
{
    T->argc = argc;
    T->argv = argv;
}

TEA_API const char** tea_get_argv(TeaState* T, int* argc)
{
    if(argc != NULL)
    {
        *argc = T->argc;
    }
    return T->argv;
}

TEA_API TeaCFunction tea_atpanic(TeaState* T, TeaCFunction panicf)
{
    TeaCFunction old = T->panic;
    T->panic = panicf;
    return old;
}

TEA_API int tea_get_top(TeaState* T)
{
    return (int)(T->top - T->base);
}

TEA_API void tea_set_top(TeaState* T, int index)
{
    if(index >= 0)
    {
        while(T->top < T->base + index)
            tea_vm_push(T, NULL_VAL);
        T->top = T->base + index;
    }
    else
    {
        T->top += index;
    }
}

TEA_API void tea_remove(TeaState* T, int index)
{
    TeaValue* p = index2stack(T, index);
    while(++p < T->top)
        *(p - 1) = *p;
    T->top--;
}

TEA_API void tea_insert(TeaState* T, int index)
{
    TeaValue* p = index2stack(T, index);
    for(TeaValue* q = T->top; q > p; q--)
        *q = *(q - 1);
    *p = *(T->top);
}

TEA_API void tea_replace(TeaState* T, int index)
{
    TeaValue* v = index2stack(T, index);
    *v = *(T->top - 1);
    T->top--;
}

TEA_API void tea_push_value(TeaState* T, int index)
{
    TeaValue value = index2value(T, index);
    tea_vm_push(T, value);
}

TEA_API int tea_type(TeaState* T, int index)
{
    TeaValue* slot = index2stack(T, index);
    if(slot == NULL)
        return TEA_TYPE_NONE;
    TeaValue value = *slot;

    if(IS_NULL(value)) return TEA_TYPE_NULL;
    if(IS_BOOL(value)) return TEA_TYPE_BOOL;
    if(IS_NUMBER(value)) return TEA_TYPE_NUMBER;
    if(IS_OBJECT(value))
    {
        switch(OBJECT_TYPE(value))
        {
            case OBJ_RANGE:
                return TEA_TYPE_RANGE;
            case OBJ_LIST:
                return TEA_TYPE_LIST;
            case OBJ_CLOSURE:
                return TEA_TYPE_FUNCTION;
            case OBJ_MAP:
                return TEA_TYPE_MAP;
            case OBJ_STRING:
                return TEA_TYPE_STRING;
            case OBJ_FILE:
                return TEA_TYPE_FILE;
            case OBJ_MODULE:
                return TEA_TYPE_MODULE;
            default:;
        }
    }
    return TEA_TYPE_NONE;
}

TEA_API const char* tea_type_name(TeaState* T, int index)
{
    TeaValue* slot = index2stack(T, index);
    return (slot == NULL) ? "no value" : tea_value_type(*slot);
}

TEA_API double tea_get_number(TeaState* T, int index)
{
    return AS_NUMBER(index2value(T, index));
}

TEA_API int tea_get_bool(TeaState* T, int index)
{
    return AS_BOOL(index2value(T, index));
}

TEA_API void tea_get_range(TeaState* T, int index, double* start, double* end, double* step)
{
    TeaObjectRange* range = AS_RANGE(index2value(T, index));
    if(start != NULL)
    {
        *start = range->start;
    }
    if(end != NULL)
    {
        *end = range->end;
    }
    if(step != NULL)
    {
        *step = range->step;
    }
}

TEA_API const char* tea_get_lstring(TeaState* T, int index, int* len)
{
    TeaObjectString* string = AS_STRING(index2value(T, index));
    if(len != NULL)
    {
        *len = string->length;
    }
    return string->chars;
}

TEA_API int tea_falsey(TeaState* T, int index)
{
    return tea_obj_falsey(index2value(T, index));
}

TEA_API double tea_to_numberx(TeaState* T, int index, int* is_num)
{
    return tea_value_tonumber(index2value(T, index), is_num);
}

TEA_API const char* tea_to_lstring(TeaState* T, int index, int* len)
{
    TeaObjectString* string = tea_value_tostring(T, index2value(T, index));
    tea_vm_push(T, OBJECT_VAL(string));
    if(len != NULL)
    {
        *len = string->length;
    }
    return string->chars;
}

TEA_API int tea_equals(TeaState* T, int index1, int index2)
{
    return tea_value_equal(index2value(T, index1), index2value(T, index2));
}

TEA_API void tea_pop(TeaState* T, int n)
{
    T->top -= n;
}

TEA_API void tea_push_null(TeaState* T)
{
    tea_vm_push(T, NULL_VAL);
}

TEA_API void tea_push_bool(TeaState* T, int b)
{
    tea_vm_push(T, BOOL_VAL(b));
}

TEA_API void tea_push_number(TeaState* T, double n)
{
    tea_vm_push(T, NUMBER_VAL(n));
}

TEA_API const char* tea_push_lstring(TeaState* T, const char* s, int len)
{
    TeaObjectString* string = (len == 0) ? tea_string_literal(T, "") : tea_string_copy(T, s, len);
    tea_vm_push(T, OBJECT_VAL(string));

    return string->chars;
}

TEA_API const char* tea_push_string(TeaState* T, const char* s)
{
    TeaObjectString* string = tea_string_new(T, s);
    tea_vm_push(T, OBJECT_VAL(string));

    return string->chars;
}

static char* format(TeaState* T, const char* fmt, va_list args, int* l)
{
    int len = vsnprintf(NULL, 0, fmt, args);
    char* msg = TEA_ALLOCATE(T, char, len + 1);
    vsnprintf(msg, len + 1, fmt, args);
    *l = len;
    return msg;
}

TEA_API const char* tea_push_fstring(TeaState* T, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len;
    char* s = format(T, fmt, args, &len);
    va_end(args);

    TeaObjectString* string = tea_string_take(T, (char*)s, len);
    tea_vm_push(T, OBJECT_VAL(string));

    return string->chars;
}

TEA_API void tea_push_range(TeaState* T, double start, double end, double step)
{
    tea_vm_push(T, OBJECT_VAL(tea_obj_new_range(T, start, end, step)));
}

TEA_API void tea_new_list(TeaState* T)
{
    tea_vm_push(T, OBJECT_VAL(tea_obj_new_list(T)));
}

TEA_API void tea_new_map(TeaState* T)
{
    tea_vm_push(T, OBJECT_VAL(tea_map_new(T)));
}

TEA_API void tea_push_cfunction(TeaState* T, TeaCFunction fn)
{
    TeaObjectNative* native = tea_obj_new_native(T, NATIVE_FUNCTION, fn);
    tea_vm_push(T, OBJECT_VAL(native));
}

static void set_class(TeaState* T, const TeaClass* k)
{
    for(; k->name != NULL; k++)
    {
        if(k->fn == NULL)
        {
            tea_push_null(T);
        }
        else
        {
            if(strcmp(k->type, "method") == 0)
            {
                tea_vm_push(T, OBJECT_VAL(tea_obj_new_native(T, NATIVE_METHOD, k->fn)));
            }
            else if(strcmp(k->type, "property") == 0)
            {
                tea_vm_push(T, OBJECT_VAL(tea_obj_new_native(T, NATIVE_PROPERTY, k->fn)));
            }
        }
        tea_set_key(T, 0, k->name);
    }
}

TEA_API void tea_create_class(TeaState* T, const char* name, const TeaClass* klass)
{
    tea_vm_push(T, OBJECT_VAL(tea_obj_new_class(T, tea_string_new(T, name), NULL)));
    if(klass != NULL)
    {
        set_class(T, klass);
    }
}

static void set_module(TeaState* T, const TeaModule* m)
{
    for(; m->name != NULL; m++)
    {
        if(m->fn == NULL)
        {
            tea_push_null(T);
        }
        else
        {
            tea_push_cfunction(T, m->fn);
        }
        tea_set_key(T, 0, m->name);
    }
}

TEA_API void tea_create_module(TeaState* T, const char* name, const TeaModule* module)
{
    TeaObjectModule* mod = tea_obj_new_module(T, tea_string_new(T, name));
    mod->path = tea_string_new(T, name);

    tea_vm_push(T, OBJECT_VAL(mod));
    if(module != NULL)
    {
        set_module(T, module);
    }
}

TEA_API int tea_len(TeaState* T, int index)
{
    TeaValue object = index2value(T, index);

    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_STRING:
            {
                return AS_STRING(object)->length;
            }
            case OBJ_LIST:
            {
                return AS_LIST(object)->items.count;
            }
            case OBJ_MAP:
            {
                return AS_MAP(object)->count;
            }
            default:;
        }
    }
    return -1;
}

TEA_API void tea_get_item(TeaState* T, int list, int index)
{
    TeaValueArray items = AS_LIST(index2value(T, list))->items;
    tea_vm_push(T, items.values[index]);
}

TEA_API void tea_set_item(TeaState* T, int list, int index)
{
    TeaObjectList* l = AS_LIST(index2value(T, list));
    l->items.values[index] = tea_vm_peek(T, 0);
    tea_pop(T, 1);
}

TEA_API void tea_add_item(TeaState* T, int list)
{
    TeaObjectList* l = AS_LIST(index2value(T, list));
    tea_write_value_array(T, &l->items, tea_vm_peek(T, 0));
    tea_pop(T, 1);
}

TEA_API void tea_get_field(TeaState* T, int map)
{
    //TeaValue value = tea_vm_peek(T, map);
}

TEA_API void tea_set_field(TeaState* T, int map)
{
    TeaValue object = index2value(T, map);
    TeaValue item = tea_vm_peek(T, 1);
    TeaValue key = tea_vm_peek(T, 2);

    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(object);
                tea_map_set(T, map, key, item);
                break;
            }
        }
    }
    tea_pop(T, 2);
}

TEA_API void tea_set_key(TeaState* T, int map, const char* key)
{
    TeaValue object = index2value(T, map);
    TeaValue item = tea_vm_peek(T, 0);

    tea_push_string(T, key);
    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(object);
                TeaObjectString* string = AS_STRING(tea_vm_peek(T, 0));
                tea_table_set(T, &module->values, string, item);
                break;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(object);
                TeaValue key = tea_vm_peek(T, 0);
                tea_map_set(T, map, key, item);
                break;
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(object);
                TeaObjectString* string = AS_STRING(tea_vm_peek(T, 0));
                tea_table_set(T, &klass->methods, string, item);
                if(strcmp(string->chars, "constructor") == 0)
                {
                    klass->constructor = item;
                }
                break;
            }
        }
    }
    tea_pop(T, 2);
}

TEA_API int tea_get_global(TeaState* T, const char* name)
{
    tea_push_string(T, name);
    TeaValue _;
    int b = tea_table_get(&T->globals, AS_STRING(tea_vm_peek(T, 0)), &_);
    tea_pop(T, 1);
    if(b)
    {
        tea_vm_push(T, _);
    }
    return b;
}

TEA_API void tea_set_global(TeaState* T, const char* name)
{
    TeaValue value = tea_vm_peek(T, 0);
    tea_push_string(T, name);
    tea_table_set(T, &T->globals, AS_STRING(tea_vm_peek(T, 0)), value);
    tea_pop(T, 2);
}

static void set_globals(TeaState* T, const TeaReg* reg)
{
    for(; reg->name != NULL; reg++)
    {
        if(reg->fn == NULL)
        {
            tea_push_null(T);
        }
        else
        {
            tea_push_cfunction(T, reg->fn);
        }
        tea_set_global(T, reg->name);
    }
}

TEA_API void tea_set_funcs(TeaState* T, const TeaReg* reg)
{
    set_globals(T, reg);
}

static void expected(TeaState* T, const char* type, int index)
{
    tea_error(T, "Expected %s, got %s", type, tea_type_name(T, index));
}

TEA_API int tea_check_type(TeaState* T, int index, int type)
{
    if(tea_type(T, index) != type)
    {
        expected(T, tea_value_typenames[type], index);
    }
}

TEA_API int tea_check_bool(TeaState* T, int index)
{
    TeaValue value = index2value(T, index);
    if(!IS_BOOL(value))
    {
        expected(T, "bool", index);
    }
    return AS_BOOL(value);
}

TEA_API void tea_check_range(TeaState* T, int index, double* start, double* end, double* step)
{
    TeaValue value = index2value(T, index);
    if(!IS_RANGE(value))
    {
        expected(T, "range", index);
    }
    tea_get_range(T, index, start, end, step);
}

TEA_API void tea_check_any(TeaState* T, int index)
{
    if(tea_type(T, index) == TEA_TYPE_NONE)
        tea_error(T, "Expected value, got none");
}

TEA_API double tea_check_number(TeaState* T, int index)
{
    TeaValue value = index2value(T, index);
    if(!IS_NUMBER(value))
    {
        expected(T, "number", index);
    }
    return AS_NUMBER(value);
}

TEA_API const char* tea_check_lstring(TeaState* T, int index, int* len)
{
    TeaValue value = index2value(T, index);
    if(!IS_STRING(value))
    {
        expected(T, "string", index);
    }
    return tea_get_lstring(T, index, len);
}

TEA_API const char* tea_opt_lstring(TeaState* T, int index, const char* def, int* len)
{
    if(tea_is_nonenull(T, index))
    {
        if(len)
            *len = (def ? strlen(def) : 0);
        return def;
    }
    else
        return tea_check_lstring(T, index, len);
}

TEA_API void tea_call(TeaState* T, int n)
{
    TeaValue func = T->top[-n - 1];
    tea_do_call(T, func, n);
}

TEA_API void tea_error(TeaState* T, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char msg[1024];
    int len = vsnprintf(NULL, 0, fmt, args);
    vsnprintf(msg, len + 1, fmt, args);
    va_end(args);

    tea_vm_runtime_error(T, msg);
}