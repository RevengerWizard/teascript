// tea_api.c
// C API functions for Teascript 

#include "tea.h"

#include "tea_state.h"
#include "tea_vm.h"

TEA_API int tea_version()
{
    return TEA_VERSION_NUMBER;
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

TEA_API int tea_get_top(TeaState* T)
{
    return T->top;
}

static TeaValue get_slot(TeaState* T, int index)
{
    return T->slot[index];
}

static void push_slot(TeaState* T, TeaValue value)
{
    T->slot[T->top++] = value;
}

static void set_slot(TeaState* T, int index, TeaValue value)
{
    T->slot[index] = value;
}

static void set_class(TeaState* T, const TeaClass* k, int pos)
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
                push_slot(T, OBJECT_VAL(tea_new_native(T, NATIVE_METHOD, k->fn)));
            }
            else if(strcmp(k->type, "property") == 0)
            {
                push_slot(T, OBJECT_VAL(tea_new_native(T, NATIVE_PROPERTY, k->fn)));
            }
        }
        tea_set_key(T, pos, k->name);
    }
}

static void set_module(TeaState* T, const TeaModule* m, int pos)
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
        tea_set_key(T, pos, m->name);
    }
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

TEA_API int tea_type(TeaState* T, int index)
{
    TeaValue value = get_slot(T, index);

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
        }
    }
    return TEA_TYPE_UNKNOWN;
}

TEA_API const char* tea_type_name(TeaState* T, int index)
{
    return tea_value_type(get_slot(T, index));
}

TEA_API double tea_get_number(TeaState* T, int index)
{
    return AS_NUMBER(get_slot(T, index));
}

TEA_API int tea_get_bool(TeaState* T, int index)
{
    return AS_BOOL(get_slot(T, index));
}

TEA_API void tea_get_range(TeaState* T, int index, double* start, double* end, double* step)
{
    TeaObjectRange* range = AS_RANGE(get_slot(T, index));
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
    TeaObjectString* string = AS_STRING(get_slot(T, index));
    if(len != NULL)
    {
        *len = string->length;
    }
    return AS_CSTRING(OBJECT_VAL(string));
}

TEA_API int tea_falsey(TeaState* T, int index)
{
    return tea_is_falsey(get_slot(T, index));
}

TEA_API double tea_to_numberx(TeaState* T, int index, int* is_num)
{
    return tea_value_tonumber(get_slot(T, index), is_num);
}

TEA_API const char* tea_to_lstring(TeaState* T, int index, int* len)
{
    TeaObjectString* string = tea_value_tostring(T, get_slot(T, index));
    push_slot(T, OBJECT_VAL(string));
    if(len != NULL)
    {
        *len = string->length;
    }
    return string->chars;
}

TEA_API int tea_equals(TeaState* T, int index1, int index2)
{
    return tea_values_equal(get_slot(T, index1), get_slot(T, index2));
}

TEA_API void tea_pop(TeaState* T, int n)
{
    T->top -= n;
}

TEA_API void tea_push_value(TeaState* T, int index)
{
    push_slot(T, get_slot(T, index));
}

TEA_API void tea_push_null(TeaState* T)
{
    push_slot(T, NULL_VAL);
}

TEA_API void tea_push_bool(TeaState* T, int b)
{
    push_slot(T, BOOL_VAL(b));
}

TEA_API void tea_push_number(TeaState* T, double n)
{
    push_slot(T, NUMBER_VAL(n));
}

TEA_API const char* tea_push_lstring(TeaState* T, const char* s, int len)
{
    TeaObjectString* string = (len == 0) ? tea_copy_string(T, "", 0) : tea_copy_string(T, s, len);
    push_slot(T, OBJECT_VAL(string));

    return string->chars;
}

TEA_API const char* tea_push_string(TeaState* T, const char* s)
{
    TeaObjectString* string = tea_new_string(T, s);
    push_slot(T, OBJECT_VAL(string));

    return string->chars;
}

static char* format(TeaState* T, const char* fmt, va_list args, int* l)
{
    int len = vsnprintf(NULL, 0, fmt, args);
    char* msg = ALLOCATE(T, char, len + 1);
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

    TeaObjectString* string = tea_take_string(T, (char*)s, len);
    push_slot(T, OBJECT_VAL(string));

    return string->chars;
}

TEA_API void tea_push_range(TeaState* T, double start, double end, double step)
{
    push_slot(T, OBJECT_VAL(tea_new_range(T, start, end, step)));
}

TEA_API void tea_push_list(TeaState* T)
{
    push_slot(T, OBJECT_VAL(tea_new_list(T)));
}

TEA_API void tea_push_map(TeaState* T)
{
    push_slot(T, OBJECT_VAL(tea_new_map(T)));
}

TEA_API void tea_push_cfunction(TeaState* T, TeaCFunction fn)
{
    TeaObjectNative* native = tea_new_native(T, NATIVE_FUNCTION, fn);
    push_slot(T, OBJECT_VAL(native));
}

TEA_API void tea_create_class(TeaState* T, const char* name, const TeaClass* class)
{
    push_slot(T, OBJECT_VAL(tea_new_class(T, tea_new_string(T, name), NULL)));
    if(class != NULL)
    {
        set_class(T, class, T->top - 1);
    }
}

TEA_API void tea_create_module(TeaState* T, const char* name, const TeaModule* module)
{
    push_slot(T, OBJECT_VAL(tea_new_module(T, tea_new_string(T, name))));
    if(module != NULL)
    {
        set_module(T, module, T->top - 1);
    }
}

TEA_API int tea_len(TeaState* T, int index)
{
    TeaValue object = get_slot(T, index);

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
        }
    }
    return -1;
}

TEA_API void tea_get_item(TeaState* T, int list, int index)
{
    TeaValueArray items = AS_LIST(get_slot(T, list))->items;
    push_slot(T, items.values[index]);
}

TEA_API void tea_set_item(TeaState* T, int list, int index)
{
    TeaObjectList* l = AS_LIST(get_slot(T, list));
    l->items.values[index] = T->slot[T->top - 1];
    tea_pop(T, 1);
}

TEA_API void tea_add_item(TeaState* T, int list)
{
    TeaObjectList* l = AS_LIST(get_slot(T, list));
    tea_write_value_array(T, &l->items, T->slot[T->top - 1]);
    tea_pop(T, 1);
}

TEA_API void tea_get_field(TeaState* T, int map)
{
    TeaValue value = get_slot(T, map);
}

TEA_API void tea_set_field(TeaState* T, int map)
{
    TeaValue object = get_slot(T, map);
    TeaValue item = get_slot(T, T->top - 1);
    TeaValue key = get_slot(T, T->top - 2);

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
    TeaValue object = get_slot(T, map);
    TeaValue item = get_slot(T, T->top - 1);

    tea_push_string(T, key);
    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_MODULE:
            {
                TeaObjectModule* module = AS_MODULE(object);
                TeaObjectString* string = AS_STRING(get_slot(T, T->top - 1));
                tea_table_set(T, &module->values, string, item);
                break;
            }
            case OBJ_MAP:
            {
                TeaObjectMap* map = AS_MAP(object);
                TeaValue key = get_slot(T, T->top - 1);
                tea_map_set(T, map, key, item);
                break;
            }
            case OBJ_CLASS:
            {
                TeaObjectClass* klass = AS_CLASS(object);
                TeaObjectString* string = AS_STRING(get_slot(T, T->top - 1));
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
    int b = tea_table_get(&T->globals, AS_STRING(get_slot(T, T->top - 1)), &_);
    tea_pop(T, 1);
    if(b)
    {
        push_slot(T, _);
    }
    return b;
}

TEA_API void tea_set_global(TeaState* T, const char* name)
{
    TeaValue value = get_slot(T, T->top - 1);
    tea_push_string(T, name);
    tea_table_set(T, &T->globals, AS_STRING(get_slot(T, T->top - 1)), value);
    tea_pop(T, 2);
}

TEA_API void tea_set_funcs(TeaState* T, const TeaReg* reg)
{
    set_globals(T, reg);
}

static void expected(TeaState* T, const char* type, int index)
{
    tea_error(T, "Expected %s, got %s", type, tea_type_name(T, index));
}

TEA_API int tea_check_bool(TeaState* T, int index)
{
    TeaValue value = get_slot(T, index);
    if(!IS_BOOL(value))
    {
        expected(T, "bool", index);
    }
    return AS_BOOL(value);
}

TEA_API void tea_check_range(TeaState* T, int index, double* start, double* end, double* step)
{
    TeaValue value = get_slot(T, index);
    if(!IS_RANGE(value))
    {
        expected(T, "range", index);
    }
    TeaObjectRange* range = AS_RANGE(value);
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

TEA_API double tea_check_number(TeaState* T, int index)
{
    TeaValue value = get_slot(T, index);
    if(!IS_NUMBER(value))
    {
        expected(T, "number", index);
    }
    return AS_NUMBER(value);
}

TEA_API const char* tea_check_lstring(TeaState* T, int index, int* len)
{
    TeaValue value = get_slot(T, index);
    if(!IS_STRING(value))
    {
        expected(T, "string", index);
    }
    TeaObjectString* string = AS_STRING(value);
    if(len != NULL)
        *len = string->length;
    return string->chars;
}

TEA_API void tea_check_list(TeaState* T, int index)
{
    TeaValue value = get_slot(T, index);
    if(!IS_LIST(value))
    {
        expected(T, "list", index);
    }
}

TEA_API void tea_check_map(TeaState* T, int index)
{
    TeaValue value = get_slot(T, index);
    if(!IS_MAP(value))
    {
        expected(T, "map", index);
    }
}

TEA_API void tea_check_file(TeaState* T, int index)
{
    TeaValue value = get_slot(T, index);
    if(!IS_FILE(value))
    {
        expected(T, "file", index);
    }
}

TEA_API void tea_error(TeaState* T, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len;
    char* s = format(T, fmt, args, &len);
    va_end(args);

    tea_runtime_error(T, s);
    FREE_ARRAY(T, char, s, len + 1);
    tea_exit_jump(T);
}