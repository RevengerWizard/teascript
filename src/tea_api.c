/*
** tea_api.c
** Public Teascript C API
*/

#define tea_api_c
#define TEA_CORE

#include "tea.h"

#include "tea_state.h"
#include "tea_str.h"
#include "tea_func.h"
#include "tea_map.h"
#include "tea_vm.h"
#include "tea_err.h"
#include "tea_gc.h"
#include "tea_lex.h"
#include "tea_tab.h"

static TValue index2value(tea_State* T, int index)
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

static TValue* index2stack(tea_State* T, int index)
{
    if(index >= 0)
    {
        TValue* value = T->base + index;
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

TEA_API void tea_set_repl(tea_State* T, bool b)
{
    T->repl = b;
}

TEA_API void tea_set_argv(tea_State* T, int argc, char** argv, int argf)
{
    T->argc = argc;
    T->argv = argv;
    T->argf = argf;
}

TEA_API tea_Alloc tea_get_allocf(tea_State* T, void** ud)
{
    tea_Alloc f;
    if(ud) *ud = T->allocd;
    f = T->allocf;
    return f;
}

TEA_API void tea_set_allocf(tea_State* T, tea_Alloc f, void* ud)
{
    if(f) T->allocf = f;
    if(ud) T->allocd = ud;
}

TEA_API int tea_get_top(tea_State* T)
{
    return (int)(T->top - T->base);
}

TEA_API void tea_set_top(tea_State* T, int index)
{
    if(index >= 0)
    {
        while(T->top < T->base + index)
            tea_vm_push(T, NULL_VAL);
        T->top = T->base + index;
    }
    else
    {
        T->top += index + 1;
    }
}

TEA_API void tea_remove(tea_State* T, int index)
{
    TValue* p = index2stack(T, index);
    while(++p < T->top)
        *(p - 1) = *p;
    T->top--;
}

TEA_API void tea_insert(tea_State* T, int index)
{
    TValue* p = index2stack(T, index);
    for(TValue* q = T->top; q > p; q--)
        *q = *(q - 1);
    *p = *(T->top);
}

TEA_API void tea_replace(tea_State* T, int index)
{
    TValue* v = index2stack(T, index);
    *v = *(T->top - 1);
    T->top--;
}

TEA_API void tea_copy(tea_State* T, int from_index, int to_index)
{
    TValue* from = index2stack(T, from_index);
    TValue* to = index2stack(T, to_index);
    *to = *(from);
}

TEA_API void tea_push_value(tea_State* T, int index)
{
    TValue value = index2value(T, index);
    tea_vm_push(T, value);
}

TEA_API int tea_get_mask(tea_State* T, int index)
{
    TValue* slot = index2stack(T, index);
    if(slot == NULL)
        return TEA_MASK_NONE;
    TValue value = *slot;

    if(IS_NULL(value)) return TEA_MASK_NULL;
    if(IS_POINTER(value)) return TEA_MASK_POINTER;
    if(IS_BOOL(value)) return TEA_MASK_BOOL;
    if(IS_NUMBER(value)) return TEA_MASK_NUMBER;
    if(IS_OBJECT(value))
    {
        switch(OBJECT_TYPE(value))
        {
            case OBJ_RANGE:
                return TEA_MASK_RANGE;
            case OBJ_LIST:
                return TEA_MASK_LIST;
            case OBJ_PROTO:
            case OBJ_FUNC:
            case OBJ_CFUNC:
                return TEA_MASK_FUNCTION;
            case OBJ_MAP:
                return TEA_MASK_MAP;
            case OBJ_STRING:
                return TEA_MASK_STRING;
            case OBJ_FILE:
                return TEA_MASK_FILE;
            case OBJ_MODULE:
                return TEA_MASK_MODULE;
            default:;
        }
    }
    return TEA_MASK_NONE;
}

TEA_API int tea_get_type(tea_State* T, int index)
{
    TValue* slot = index2stack(T, index);
    if(slot == NULL)
        return TEA_TYPE_NONE;
    TValue value = *slot;

    if(IS_NULL(value)) return TEA_TYPE_NULL;
    if(IS_POINTER(value)) return TEA_TYPE_POINTER;
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
            case OBJ_PROTO:
            case OBJ_FUNC:
            case OBJ_CFUNC:
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

TEA_API const char* tea_typeof(tea_State* T, int index)
{
    TValue* slot = index2stack(T, index);
    return (slot == NULL) ? "no value" : tea_val_type(*slot);
}

TEA_API double tea_get_number(tea_State* T, int index)
{
    return AS_NUMBER(index2value(T, index));
}

TEA_API bool tea_get_bool(tea_State* T, int index)
{
    return AS_BOOL(index2value(T, index));
}

TEA_API void tea_get_range(tea_State* T, int index, double* start, double* end, double* step)
{
    GCrange* range = AS_RANGE(index2value(T, index));
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

TEA_API const char* tea_get_lstring(tea_State* T, int index, int* len)
{
    GCstr* string = AS_STRING(index2value(T, index));
    if(len != NULL)
    {
        *len = string->len;
    }
    return string->chars;
}

TEA_API const void* tea_get_pointer(tea_State* T, int index)
{
    return AS_POINTER(index2value(T, index));
}

TEA_API bool tea_is_object(tea_State* T, int index)
{
    TValue v = index2value(T, index);
    return IS_OBJECT(v);
}

TEA_API bool tea_is_cfunction(tea_State* T, int index)
{
    TValue v = index2value(T, index);
    return IS_CFUNC(v);
}

TEA_API bool tea_to_bool(tea_State* T, int index)
{
    TValue* v = index2stack(T, index);
    return (v == NULL) ? false : !tea_obj_isfalse(*v);
}

TEA_API double tea_to_numberx(tea_State* T, int index, bool* is_num)
{
    TValue* v = index2stack(T, index);
    if(v == NULL)
    {
        if(is_num != NULL) *is_num = false;
        return 0;
    }
    return tea_val_tonumber(*v, is_num);
}

TEA_API const char* tea_to_lstring(tea_State* T, int index, int* len)
{
    TValue* value = index2stack(T, index);
    if(value == NULL)
    {
        if(len != NULL) *len = 0;
        return NULL;
    }

    GCstr* string = tea_val_tostring(T, *value, 0);
    tea_vm_push(T, OBJECT_VAL(string));
    if(len != NULL)
    {
        *len = string->len;
    }
    return string->chars;
}

TEA_API tea_CFunction tea_to_cfunction(tea_State* T, int index)
{
    TValue v = index2value(T, index);
    tea_CFunction f = NULL;

    if(IS_CFUNC(v))
        f = AS_CFUNC(v)->fn;

    return f;
}

TEA_API const void* tea_to_pointer(tea_State* T, int index)
{
    TValue v = index2value(T, index);
    void* p = NULL;

    if(IS_POINTER(v))
        p = AS_POINTER(v);

    return p;
}

TEA_API bool tea_equal(tea_State* T, int index1, int index2)
{
    return tea_val_equal(index2value(T, index1), index2value(T, index2));
}

TEA_API bool tea_rawequal(tea_State* T, int index1, int index2)
{
    return tea_val_rawequal(index2value(T, index1), index2value(T, index2));
}

TEA_API void tea_concat(tea_State* T)
{
    GCstr* s1 = AS_STRING(tea_vm_peek(T, 1));
    GCstr* s2 = AS_STRING(tea_vm_peek(T, 0));

    GCstr* str = tea_buf_cat2str(T, s1, s2);

    tea_vm_pop(T, 2);
    tea_vm_push(T, OBJECT_VAL(str));
}

TEA_API void tea_pop(tea_State* T, int n)
{
    T->top -= n;
}

TEA_API void tea_push_pointer(tea_State* T, void* p)
{
    tea_vm_push(T, POINTER_VAL(p));
}

TEA_API void tea_push_null(tea_State* T)
{
    tea_vm_push(T, NULL_VAL);
}

TEA_API void tea_push_true(tea_State* T)
{
    tea_vm_push(T, TRUE_VAL);
}

TEA_API void tea_push_false(tea_State* T)
{
    tea_vm_push(T, FALSE_VAL);
}

TEA_API void tea_push_bool(tea_State* T, bool b)
{
    tea_vm_push(T, BOOL_VAL(b));
}

TEA_API void tea_push_number(tea_State* T, double n)
{
    tea_vm_push(T, NUMBER_VAL(n));
}

TEA_API const char* tea_push_lstring(tea_State* T, const char* s, int len)
{
    GCstr* string = (len == 0) ? tea_str_lit(T, "") : tea_str_copy(T, s, len);
    tea_vm_push(T, OBJECT_VAL(string));

    return string->chars;
}

TEA_API const char* tea_push_string(tea_State* T, const char* s)
{
    GCstr* string = tea_str_new(T, s);
    tea_vm_push(T, OBJECT_VAL(string));

    return string->chars;
}

static char* format(tea_State* T, const char* fmt, va_list args, int* l)
{
    int len = vsnprintf(NULL, 0, fmt, args);
    char* msg = tea_mem_new(T, char, len + 1);
    vsnprintf(msg, len + 1, fmt, args);
    *l = len;
    return msg;
}

TEA_API const char* tea_push_fstring(tea_State* T, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len;
    char* s = format(T, fmt, args, &len);
    va_end(args);

    GCstr* string = tea_str_take(T, (char*)s, len);
    tea_vm_push(T, OBJECT_VAL(string));

    return string->chars;
}

TEA_API const char* tea_push_vfstring(tea_State* T, const char* fmt, va_list args)
{
    int len;
    char* s = format(T, fmt, args, &len);

    GCstr* string = tea_str_take(T, (char*)s, len);
    tea_vm_push(T, OBJECT_VAL(string));

    return string->chars;
}

TEA_API void tea_push_range(tea_State* T, double start, double end, double step)
{
    tea_vm_push(T, OBJECT_VAL(tea_obj_new_range(T, start, end, step)));
}

TEA_API void tea_new_list(tea_State* T)
{
    tea_vm_push(T, OBJECT_VAL(tea_obj_new_list(T)));
}

TEA_API void tea_new_map(tea_State* T)
{
    tea_vm_push(T, OBJECT_VAL(tea_map_new(T)));
}

TEA_API void tea_push_cfunction(tea_State* T, tea_CFunction fn, int nargs)
{
    GCfuncC* f = tea_func_newC(T, C_FUNCTION, fn, nargs);
    tea_vm_push(T, OBJECT_VAL(f));
}

static void set_class(tea_State* T, const tea_Class* k)
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
                tea_vm_push(T, OBJECT_VAL(tea_func_newC(T, C_METHOD, k->fn, k->nargs)));
            }
            else if(strcmp(k->type, "property") == 0)
            {
                tea_vm_push(T, OBJECT_VAL(tea_func_newC(T, C_PROPERTY, k->fn, k->nargs)));
            }
            else if(strcmp(k->type, "static") == 0)
            {
                tea_vm_push(T, OBJECT_VAL(tea_func_newC(T, C_FUNCTION, k->fn, k->nargs)));
            }
        }
        tea_set_key(T, -2, k->name);
    }
}

TEA_API void tea_create_class(tea_State* T, const char* name, const tea_Class* klass)
{
    tea_vm_push(T, OBJECT_VAL(tea_obj_new_class(T, tea_str_new(T, name), NULL)));
    if(klass != NULL)
    {
        set_class(T, klass);
    }
}

static void set_module(tea_State* T, const tea_Module* m)
{
    for(; m->name != NULL; m++)
    {
        if(m->fn == NULL)
        {
            tea_push_null(T);
        }
        else
        {
            tea_push_cfunction(T, m->fn, m->nargs);
        }
        tea_set_key(T, -2, m->name);
    }
}

TEA_API void tea_create_module(tea_State* T, const char* name, const tea_Module* module)
{
    GCstr* modname = tea_str_new(T, name);
    GCmodule* mod = tea_obj_new_module(T, modname);
    mod->path = modname;

    tea_vm_push(T, OBJECT_VAL(mod));
    if(module != NULL)
    {
        set_module(T, module);
    }
}

TEA_API int tea_len(tea_State* T, int index)
{
    TValue object = index2value(T, index);

    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_STRING:
            {
                return AS_STRING(object)->len;
            }
            case OBJ_LIST:
            {
                return AS_LIST(object)->count;
            }
            case OBJ_MAP:
            {
                return AS_MAP(object)->count;
            }
            default:
                break;
        }
    }
    return -1;
}

TEA_API bool tea_get_item(tea_State* T, int list, int index)
{
    GClist* l = AS_LIST(index2value(T, list));
    TValue* items = l->items;
    if(index < 0 || index > l->count)
        return false;

    tea_vm_push(T, items[index]);
    return true;
}

TEA_API void tea_set_item(tea_State* T, int list, int index)
{
    GClist* l = AS_LIST(index2value(T, list));
    l->items[index] = tea_vm_peek(T, 0);
    tea_pop(T, 1);
}

TEA_API void tea_add_item(tea_State* T, int list)
{
    GClist* l = AS_LIST(index2value(T, list));
    tea_list_append(T, l, tea_vm_peek(T, 0));
    tea_pop(T, 1);
}

TEA_API bool tea_get_field(tea_State* T, int obj)
{
    TValue object = index2value(T, obj);
    TValue key = tea_vm_peek(T, 0);
    bool found = false;

    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_MAP:
            {
                GCmap* map = AS_MAP(object);
                TValue v;
                if(tea_map_get(map, key, &v))
                {
                    T->top--;
                    tea_vm_push(T, v);
                    found = true;
                }
                break;
            }
            default:
                break;
        }
    }
    return found;
}

TEA_API void tea_set_field(tea_State* T, int obj)
{
    TValue object = index2value(T, obj);
    TValue item = tea_vm_peek(T, 0);
    TValue key = tea_vm_peek(T, 1);

    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_MAP:
            {
                GCmap* map = AS_MAP(object);
                tea_map_set(T, map, key, item);
                break;
            }
            default:
                break;
        }
    }
    tea_pop(T, 2);
}

TEA_API void tea_set_key(tea_State* T, int obj, const char* key)
{
    TValue object = index2value(T, obj);
    TValue item = tea_vm_peek(T, 0);

    tea_push_string(T, key);
    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_MODULE:
            {
                GCmodule* module = AS_MODULE(object);
                GCstr* string = AS_STRING(tea_vm_peek(T, 0));
                tea_tab_set(T, &module->values, string, item);
                break;
            }
            case OBJ_MAP:
            {
                GCmap* map = AS_MAP(object);
                TValue key = tea_vm_peek(T, 0);
                tea_map_set(T, map, key, item);
                break;
            }
            case OBJ_CLASS:
            {
                GCclass* klass = AS_CLASS(object);
                GCstr* string = AS_STRING(tea_vm_peek(T, 0));
                tea_tab_set(T, &klass->methods, string, item);
                if(string == T->constructor_string)
                {
                    klass->constructor = item;
                }
                break;
            }
            case OBJ_INSTANCE:
            {
                GCinstance* instance = AS_INSTANCE(object);
                GCstr* string = AS_STRING(tea_vm_peek(T, 0));
                tea_tab_set(T, &instance->fields, string, item);
            }
            default:
                break;
        }
    }
    tea_pop(T, 2);
}

TEA_API bool tea_get_key(tea_State* T, int obj, const char* key)
{
    TValue object = index2value(T, obj);
    bool found = false;

    tea_push_string(T, key);
    if(IS_OBJECT(object))
    {
        switch(OBJECT_TYPE(object))
        {
            case OBJ_MODULE:
            {
                GCmodule* module = AS_MODULE(object);
                GCstr* string = AS_STRING(tea_vm_peek(T, 0));
                TValue v;
                if(tea_tab_get(&module->values, string, &v))
                {
                    T->top--;
                    tea_vm_push(T, v);
                    found = true;
                }
                break;
            }
            case OBJ_MAP:
            {
                GCmap* map = AS_MAP(object);
                TValue key = tea_vm_peek(T, 0);
                TValue v;
                if(tea_map_get(map, key, &v))
                {
                    T->top--;
                    tea_vm_push(T, v);
                    found = true;
                }
                break;
            }
            case OBJ_INSTANCE:
            {
                GCinstance* instance = AS_INSTANCE(object);
                GCstr* string = AS_STRING(tea_vm_peek(T, 0));
                TValue v;
                if(tea_tab_get(&instance->fields, string, &v))
                {
                    T->top--;
                    tea_vm_push(T, v);
                    found = true;
                }
            }
            default:
                break;
        }
    }
    return found;
}

TEA_API bool tea_get_global(tea_State* T, const char* name)
{
    tea_push_string(T, name);
    TValue _;
    bool b = tea_tab_get(&T->globals, AS_STRING(tea_vm_peek(T, 0)), &_);
    tea_pop(T, 1);
    if(b)
    {
        tea_vm_push(T, _);
    }
    return b;
}

TEA_API void tea_set_global(tea_State* T, const char* name)
{
    TValue value = tea_vm_peek(T, 0);
    tea_push_string(T, name);
    tea_tab_set(T, &T->globals, AS_STRING(tea_vm_peek(T, 0)), value);
    tea_pop(T, 2);
}

static void set_globals(tea_State* T, const tea_Reg* reg)
{
    for(; reg->name != NULL; reg++)
    {
        if(reg->fn == NULL)
        {
            tea_push_null(T);
        }
        else
        {
            tea_push_cfunction(T, reg->fn, reg->nargs);
        }
        tea_set_global(T, reg->name);
    }
}

TEA_API void tea_set_funcs(tea_State* T, const tea_Reg* reg)
{
    set_globals(T, reg);
}

TEA_API bool tea_has_module(tea_State* T, const char* module)
{
    GCstr* modname = tea_str_new(T, module);
    tea_vm_push(T, OBJECT_VAL(modname));

    TValue _;
    bool found = tea_tab_get(&T->modules, modname, &_);
    tea_pop(T, 1);

    return found;
}

TEA_API bool tea_test_stack(tea_State* T, int size)
{
    bool res = true;
    if(size > TEA_MAX_CSTACK || (T->top - T->base + size) > TEA_MAX_CSTACK)
    {
        res = false;
    }
    else if(size > 0)
    {
        tea_state_checkstack(T, size);
    }
    return res;
}

TEA_API void tea_check_stack(tea_State* T, int size, const char* msg)
{
    if(!tea_test_stack(T, size))
    {
        tea_error(T, "Stack overflow, %s", msg);
    }
}

static void expected(tea_State* T, const char* type, int index)
{
    tea_error(T, "Expected %s, got %s", type, tea_typeof(T, index));
}

TEA_API void tea_check_type(tea_State* T, int index, int type)
{
    if(tea_get_type(T, index) != type)
    {
        expected(T, tea_val_typenames[type], index);
    }
}

TEA_API bool tea_check_bool(tea_State* T, int index)
{
    TValue value = index2value(T, index);
    if(!IS_BOOL(value))
    {
        expected(T, "bool", index);
    }
    return AS_BOOL(value);
}

TEA_API void tea_check_range(tea_State* T, int index, double* start, double* end, double* step)
{
    TValue value = index2value(T, index);
    if(!IS_RANGE(value))
    {
        expected(T, "range", index);
    }
    tea_get_range(T, index, start, end, step);
}

TEA_API void tea_check_any(tea_State* T, int index)
{
    if(tea_get_type(T, index) == TEA_TYPE_NONE)
        tea_error(T, "Expected value, got none");
}

TEA_API double tea_check_number(tea_State* T, int index)
{
    TValue value = index2value(T, index);
    if(!IS_NUMBER(value))
    {
        expected(T, "number", index);
    }
    return AS_NUMBER(value);
}

TEA_API const char* tea_check_lstring(tea_State* T, int index, int* len)
{
    TValue value = index2value(T, index);
    if(!IS_STRING(value))
    {
        expected(T, "string", index);
    }
    return tea_get_lstring(T, index, len);
}

TEA_API tea_CFunction tea_check_cfunction(tea_State* T, int index)
{
    TValue value = index2value(T, index);
    if(!IS_CFUNC(value))
    {
        expected(T, "cfunction", index);
    }
    return AS_CFUNC(value)->fn;
}

TEA_API const void* tea_check_pointer(tea_State* T, int index)
{
    TValue value = index2value(T, index);
    if(!IS_POINTER(value))
    {
        expected(T, "pointer", index);
    }
    return AS_POINTER(value);
}

TEA_API void tea_opt_any(tea_State* T, int index)
{
    if(tea_is_none(T, index))
    {
        tea_push_null(T);
    }
    else {}
}

TEA_API bool tea_opt_bool(tea_State* T, int index, bool def)
{
    return tea_is_nonenull(T, index) ? (def) : tea_check_bool(T, index);
}

TEA_API double tea_opt_number(tea_State* T, int index, double def)
{
    return tea_is_nonenull(T, index) ? (def) : tea_check_number(T, index);
}

TEA_API const char* tea_opt_lstring(tea_State* T, int index, const char* def, int* len)
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

TEA_API const void* tea_opt_pointer(tea_State* T, int index, void* def)
{
    return tea_is_nonenull(T, index) ? (def) : tea_check_pointer(T, index);
}

TEA_API int tea_check_option(tea_State* T, int index, const char* def, const char* const options[])
{
    const char* name = (def) ? tea_opt_string(T, index, def) : tea_check_string(T, index);
    for(int i = 0; options[i]; i++)
    {
        if(strcmp(options[i], name) == 0)
            return i;
    }

    tea_error(T, "Invalid option '%s'", name);
    return 0;
}

TEA_API int tea_gc(tea_State* T)
{
    size_t before = T->bytes_allocated;
    tea_gc_collect(T);
    size_t collected = before - T->bytes_allocated;

    /* GC values are expressed in Kbytes: number of bytes / 2 ** 10 */
    return collected >> 10;
}

typedef struct CallCtx
{
    TValue func;
    int arg_count;
} CallCtx;

static void call_f(tea_State* T, void* ud)
{
    CallCtx* ctx = (CallCtx*)ud;
    tea_vm_call(T, ctx->func, ctx->arg_count);
}

TEA_API void tea_call(tea_State* T, int n)
{
    TValue func = T->top[-n - 1];
    tea_vm_call(T, func, n);
}

TEA_API int tea_pcall(tea_State* T, int n)
{
    CallCtx ctx;
    ctx.func = T->top[-n - 1];
    ctx.arg_count = n;

    TValue* f = T->top - n - 1;
    return tea_vm_pcall(T, call_f, &ctx, stack_save(T, f));
}