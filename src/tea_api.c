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
#include "tea_list.h"

/* -- Common helper functions --------------------------------------------- */

static TValue* index2addr(tea_State* T, int index)
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

/* -- Stack manipulation -------------------------------------------------- */

TEA_API int tea_get_top(tea_State* T)
{
    return (int)(T->top - T->base);
}

TEA_API void tea_set_top(tea_State* T, int index)
{
    if(index >= 0)
    {
        while(T->top < T->base + index)
            setnullV(T->top++);
        T->top = T->base + index;
    }
    else
    {
        T->top += index + 1;
    }
}

TEA_API void tea_remove(tea_State* T, int index)
{
    TValue* p = index2addr(T, index);
    while(++p < T->top) copyTV(T, p - 1, p);
    T->top--;
}

TEA_API void tea_insert(tea_State* T, int index)
{
    TValue* p = index2addr(T, index);
    for(TValue* q = T->top; q > p; q--) copyTV(T, q, q - 1);
    copyTV(T, p, T->top);
}

static void copy_slot(tea_State* T, TValue* f, int index)
{
    TValue* o = index2addr(T, index);
    copyTV(T, o, f);
}

TEA_API void tea_replace(tea_State* T, int index)
{
    copy_slot(T, T->top - 1, index);
    T->top--;
}

TEA_API void tea_copy(tea_State* T, int from_index, int to_index)
{
    copy_slot(T, index2addr(T, from_index), to_index);
}

TEA_API void tea_push_value(tea_State* T, int index)
{
    copyTV(T, T->top, index2addr(T, index));
    T->top++;
}

/* -- Stack getters ------------------------------------------------------ */

TEA_API int tea_get_mask(tea_State* T, int index)
{
    TValue* value = index2addr(T, index);
    if(value == NULL)
        return TEA_MASK_NONE;
    switch(itype(value))
    {
        case TEA_TNULL:
            return TEA_MASK_NULL;
        case TEA_TPOINTER:
            return TEA_MASK_POINTER;
        case TEA_TBOOL:
            return TEA_MASK_BOOL;
        case TEA_TNUMBER:
            return TEA_MASK_NUMBER;
        case TEA_TRANGE:
            return TEA_MASK_RANGE;
        case TEA_TLIST:
            return TEA_MASK_LIST;
        case TEA_TPROTO:
        case TEA_TFUNC:
        case TEA_TCFUNC:
            return TEA_MASK_FUNCTION;
        case TEA_TMAP:
            return TEA_MASK_MAP;
        case TEA_TSTRING:
            return TEA_MASK_STRING;
        case TEA_TFILE:
            return TEA_MASK_FILE;
        case TEA_TMODULE:
            return TEA_MASK_MODULE;
        default:
            break;
    }
    return TEA_MASK_NONE;
}

TEA_API int tea_get_type(tea_State* T, int index)
{
    TValue* value = index2addr(T, index);
    if(value == NULL)
        return TEA_TYPE_NONE;
    switch(itype(value))
    {
        case TEA_TNULL:
            return TEA_TYPE_NULL;
        case TEA_TBOOL:
            return TEA_TYPE_BOOL;
        case TEA_TNUMBER:
            return TEA_TYPE_NUMBER;
        case TEA_TPOINTER:
            return TEA_TYPE_POINTER;
        case TEA_TRANGE:
            return TEA_TYPE_RANGE;
        case TEA_TLIST:
            return TEA_TYPE_LIST;
        case TEA_TPROTO:
        case TEA_TFUNC:
        case TEA_TCFUNC:
            return TEA_TYPE_FUNCTION;
        case TEA_TMAP:
            return TEA_TYPE_MAP;
        case TEA_TSTRING:
            return TEA_TYPE_STRING;
        case TEA_TFILE:
            return TEA_TYPE_FILE;
        case TEA_TMODULE:
            return TEA_TYPE_MODULE;
        default:
            break;
    }
    return TEA_TYPE_NONE;
}

TEA_API const char* tea_typeof(tea_State* T, int index)
{
    TValue* slot = index2addr(T, index);
    return (slot == NULL) ? "no value" : tea_typename(slot);
}

TEA_API double tea_get_number(tea_State* T, int index)
{
    return numberV(index2addr(T, index));
}

TEA_API bool tea_get_bool(tea_State* T, int index)
{
    return boolV(index2addr(T, index));
}

TEA_API void tea_get_range(tea_State* T, int index, double* start, double* end, double* step)
{
    GCrange* range = rangeV(index2addr(T, index));
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
    GCstr* string = strV(index2addr(T, index));
    if(len != NULL)
    {
        *len = string->len;
    }
    return string->chars;
}

TEA_API const void* tea_get_pointer(tea_State* T, int index)
{
    return pointerV(index2addr(T, index));
}

TEA_API bool tea_is_object(tea_State* T, int index)
{
    TValue* v = index2addr(T, index);
    return tvisgcv(v);
}

TEA_API bool tea_is_cfunction(tea_State* T, int index)
{
    TValue* v = index2addr(T, index);
    return tviscfunc(v);
}

TEA_API bool tea_to_bool(tea_State* T, int index)
{
    TValue* v = index2addr(T, index);
    return (v == NULL) ? false : !tea_obj_isfalse(v);
}

TEA_API double tea_to_numberx(tea_State* T, int index, bool* is_num)
{
    TValue* v = index2addr(T, index);
    if(v == NULL)
    {
        if(is_num != NULL) *is_num = false;
        return 0;
    }
    return tea_val_tonumber(v, is_num);
}

TEA_API const char* tea_to_lstring(tea_State* T, int index, int* len)
{
    TValue* value = index2addr(T, index);
    if(value == NULL)
    {
        if(len != NULL) *len = 0;
        return NULL;
    }

    GCstr* str = tea_val_tostring(T, value, 0);
    setstrV(T, T->top, str);
    T->top++;

    if(len != NULL)
    {
        *len = str->len;
    }
    return str->chars;
}

TEA_API tea_CFunction tea_to_cfunction(tea_State* T, int index)
{
    TValue* v = index2addr(T, index);
    tea_CFunction f = NULL;
    if(tviscfunc(v))
        f = cfuncV(v)->fn;
    return f;
}

TEA_API const void* tea_to_pointer(tea_State* T, int index)
{
    TValue* v = index2addr(T, index);
    return tea_obj_pointer(v);
}

TEA_API bool tea_equal(tea_State* T, int index1, int index2)
{
    return tea_val_equal(index2addr(T, index1), index2addr(T, index2));
}

TEA_API bool tea_rawequal(tea_State* T, int index1, int index2)
{
    return tea_val_rawequal(index2addr(T, index1), index2addr(T, index2));
}

TEA_API void tea_concat(tea_State* T)
{
    GCstr* s1 = strV(T->top - 2);
    GCstr* s2 = strV(T->top - 1);

    GCstr* str = tea_buf_cat2str(T, s1, s2);

    T->top -= 2;
    setstrV(T, T->top, str);
    T->top++;
}

TEA_API void tea_pop(tea_State* T, int n)
{
    T->top -= n;
}

/* -- Stack setters (object creation) -------------------------------------------------- */

TEA_API void tea_push_pointer(tea_State* T, void* p)
{
    setpointerV(T->top, p);
    T->top++;
}

TEA_API void tea_push_null(tea_State* T)
{
    setnullV(T->top);
    T->top++;
}

TEA_API void tea_push_true(tea_State* T)
{
    settrueV(T->top);
    T->top++;
}

TEA_API void tea_push_false(tea_State* T)
{
    setfalseV(T->top);
    T->top++;
}

TEA_API void tea_push_bool(tea_State* T, bool b)
{
    setboolV(T->top, b);
    T->top++;
}

TEA_API void tea_push_number(tea_State* T, double n)
{
    setnumberV(T->top, n);
    T->top++;
}

TEA_API const char* tea_push_lstring(tea_State* T, const char* s, int len)
{
    GCstr* str = (len == 0) ? tea_str_newlit(T, "") : tea_str_copy(T, s, len);
    setstrV(T, T->top, str);
    T->top++;
    return str->chars;
}

TEA_API const char* tea_push_string(tea_State* T, const char* s)
{
    GCstr* str = tea_str_new(T, s);
    setstrV(T, T->top, str);
    T->top++;
    return str->chars;
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

    GCstr* str = tea_str_take(T, (char*)s, len);
    setstrV(T, T->top, str);
    T->top++;

    return str->chars;
}

TEA_API const char* tea_push_vfstring(tea_State* T, const char* fmt, va_list args)
{
    int len;
    char* s = format(T, fmt, args, &len);

    GCstr* str = tea_str_take(T, (char*)s, len);
    setstrV(T, T->top, str);
    T->top++;

    return str->chars;
}

TEA_API void tea_push_range(tea_State* T, double start, double end, double step)
{
    GCrange* r = tea_obj_new_range(T, start, end, step);
    setrangeV(T, T->top, r);
    T->top++;
}

TEA_API void tea_new_list(tea_State* T)
{
    GClist* l = tea_list_new(T);
    setlistV(T, T->top, l);
    T->top++;
}

TEA_API void tea_new_map(tea_State* T)
{
    GCmap* m = tea_map_new(T);
    setmapV(T, T->top, m);
    T->top++;
}

TEA_API void tea_push_cfunction(tea_State* T, tea_CFunction fn, int nargs)
{
    GCfuncC* cf = tea_func_newC(T, C_FUNCTION, fn, nargs);
    setcfuncV(T, T->top, cf);
    T->top++;
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
                GCfuncC* cf = tea_func_newC(T, C_METHOD, k->fn, k->nargs);
                setcfuncV(T, T->top++, cf);
            }
            else if(strcmp(k->type, "property") == 0)
            {
                GCfuncC* cf = tea_func_newC(T, C_PROPERTY, k->fn, k->nargs);
                setcfuncV(T, T->top++, cf);
            }
            else if(strcmp(k->type, "static") == 0)
            {
                GCfuncC* cf = tea_func_newC(T, C_FUNCTION, k->fn, k->nargs);
                setcfuncV(T, T->top++, cf);
            }
        }
        tea_set_key(T, -2, k->name);
    }
}

TEA_API void tea_create_class(tea_State* T, const char* name, const tea_Class* klass)
{
    GCclass* k = tea_obj_new_class(T, tea_str_new(T, name), NULL);
    setclassV(T, T->top, k);
    T->top++;

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

    setmoduleV(T, T->top, mod);
    T->top++;

    if(module != NULL)
    {
        set_module(T, module);
    }
}

/* -- Object getters -------------------------------------------------- */

TEA_API int tea_len(tea_State* T, int index)
{
    TValue* object = index2addr(T, index);
    switch(itype(object))
    {
        case TEA_TSTRING:
        {
            return strV(object)->len;
        }
        case TEA_TLIST:
        {
            return listV(object)->count;
        }
        case TEA_TMAP:
        {
            return mapV(object)->count;
        }
        default:
            break;
    }
    return -1;
}

TEA_API bool tea_get_item(tea_State* T, int list, int index)
{
    GClist* l = listV(index2addr(T, list));
    TValue* items = l->items;
    if(index < 0 || index > l->count)
        return false;

    copyTV(T, T->top++, items + index);
    return true;
}

TEA_API void tea_set_item(tea_State* T, int list, int index)
{
    GClist* l = listV(index2addr(T, list));
    copyTV(T, l->items + index, T->top - 1);
    T->top--;
}

TEA_API void tea_add_item(tea_State* T, int list)
{
    GClist* l = listV(index2addr(T, list));
    tea_list_append(T, l, T->top - 1);
    T->top--;
}

TEA_API bool tea_get_field(tea_State* T, int obj)
{
    TValue* object = index2addr(T, obj);
    TValue* key = T->top - 1;
    bool found = false;

    switch(itype(object))
    {
        case TEA_TMAP:
        {
            GCmap* map = mapV(object);
            TValue* v = tea_map_get(map, key);
            if(v)
            {
                T->top--;
                copyTV(T, T->top++, v);
                found = true;
            }
            break;
        }
        default:
            break;
    }
    return found;
}

TEA_API void tea_set_field(tea_State* T, int obj)
{
    TValue* object = index2addr(T, obj);
    TValue* item = T->top - 1;
    TValue* key = T->top - 2;

    switch(itype(object))
    {
        case TEA_TMAP:
        {
            GCmap* map = mapV(object);
            TValue* v = tea_map_set(T, map, key);
            copyTV(T, v, item);
            break;
        }
        default:
            break;
    }
    T->top -= 2;
}

TEA_API void tea_set_key(tea_State* T, int obj, const char* key)
{
    TValue* object = index2addr(T, obj);
    TValue* item = T->top - 1;

    tea_push_string(T, key);
    switch(itype(object))
    {
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(object);
            GCstr* string = strV(T->top - 1);
            TValue* v = tea_tab_set(T, &module->values, string, NULL);
            copyTV(T, v, item);
            break;
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(object);
            TValue* key = T->top - 1;
            TValue* v = tea_map_set(T, map, key);
            copyTV(T, v, item);
            break;
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(object);
            GCstr* string = strV(T->top - 1);
            TValue* v = tea_tab_set(T, &klass->methods, string, NULL);
            copyTV(T, v, item);
            if(string == T->constructor_string)
            {
                copyTV(T, &klass->constructor, item);
            }
            break;
        }
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(object);
            GCstr* string = strV(T->top - 1);
            TValue* v = tea_tab_set(T, &instance->fields, string, NULL);
            copyTV(T, v, item);
            break;
        }
        default:
            break;
    }
    T->top -= 2;
}

TEA_API bool tea_get_key(tea_State* T, int obj, const char* key)
{
    TValue* object = index2addr(T, obj);
    bool found = false;

    tea_push_string(T, key);
    switch(itype(object))
    {
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(object);
            GCstr* string = strV(T->top - 1);
            TValue* v = tea_tab_get(&module->values, string);
            if(v)
            {
                T->top--;
                copyTV(T, T->top++, v);
                found = true;
            }
            break;
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(object);
            TValue* key = T->top - 1;
            TValue* v = tea_map_get(map, key);
            if(v)
            {
                T->top--;
                copyTV(T, T->top++, v);
                found = true;
            }
            break;
        }
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(object);
            GCstr* string = strV(T->top - 1);
            TValue* v = tea_tab_get(&instance->fields, string);
            if(v)
            {
                T->top--;
                copyTV(T, T->top++, v);
                found = true;
            }
        }
        default:
            break;
    }
    return found;
}

TEA_API bool tea_get_global(tea_State* T, const char* name)
{
    bool b = false;
    tea_push_string(T, name);
    TValue* v = tea_tab_get(&T->globals, strV(T->top - 1));
    T->top--;
    if(v)
    {
        b = true;
        copyTV(T, T->top++, v);
    }
    return b;
}

TEA_API void tea_set_global(tea_State* T, const char* name)
{
    TValue* value = T->top - 1;
    tea_push_string(T, name);
    TValue* o = tea_tab_set(T, &T->globals, strV(T->top - 1), NULL);
    copyTV(T, o, value);
    T->top -= 2;
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
    setstrV(T, T->top++, modname);

    TValue* v = tea_tab_get(&T->modules, modname);
    T->top--;

    return v == NULL;
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
    TValue* value = index2addr(T, index);
    if(!tvisbool(value))
    {
        expected(T, "bool", index);
    }
    return boolV(value);
}

TEA_API void tea_check_range(tea_State* T, int index, double* start, double* end, double* step)
{
    TValue* value = index2addr(T, index);
    if(!tvisrange(value))
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
    TValue* value = index2addr(T, index);
    if(!tvisnumber(value))
    {
        expected(T, "number", index);
    }
    return numberV(value);
}

TEA_API const char* tea_check_lstring(tea_State* T, int index, int* len)
{
    TValue* value = index2addr(T, index);
    if(!tvisstr(value))
    {
        expected(T, "string", index);
    }
    return tea_get_lstring(T, index, len);
}

TEA_API tea_CFunction tea_check_cfunction(tea_State* T, int index)
{
    TValue* value = index2addr(T, index);
    if(!tviscfunc(value))
    {
        expected(T, "cfunction", index);
    }
    return cfuncV(value)->fn;
}

TEA_API const void* tea_check_pointer(tea_State* T, int index)
{
    TValue* value = index2addr(T, index);
    if(!tvispointer(value))
    {
        expected(T, "pointer", index);
    }
    return pointerV(value);
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

/* -- GC and memory management -------------------------------------------------- */

TEA_API int tea_gc(tea_State* T)
{
    size_t before = T->gc.bytes_allocated;
    tea_gc_collect(T);
    size_t collected = before - T->gc.bytes_allocated;

    /* GC values are expressed in Kbytes: number of bytes / 2 ** 10 */
    return collected >> 10;
}

/* -- Calls -------------------------------------------------- */

typedef struct CallCtx
{
    TValue* func;
    int arg_count;
} CallCtx;

static void call_f(tea_State* T, void* ud)
{
    CallCtx* ctx = (CallCtx*)ud;
    tea_vm_call(T, ctx->func, ctx->arg_count);
}

TEA_API void tea_call(tea_State* T, int n)
{
    TValue* func = T->top - n - 1;
    tea_vm_call(T, func, n);
}

TEA_API int tea_pcall(tea_State* T, int n)
{
    CallCtx ctx;
    ctx.func = T->top - n - 1;
    ctx.arg_count = n;

    return tea_vm_pcall(T, call_f, &ctx, stack_save(T, ctx.func));
}