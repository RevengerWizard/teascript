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
#include "tea_strfmt.h"
#include "tea_udata.h"

/* -- Common helper functions --------------------------------------------- */

#define tea_checkapi_slot(index) \
    tea_checkapi((index) <= (T->top - T->base), "stack slot #%d out of range", (index))

static TValue* index2addr(tea_State* T, int index)
{
    if(index >= 0)
    {
        TValue* o = T->base + index;
        if(o >= T->top)
            return niltv(T);
        else
            return o;
    }
    else if(index > TEA_UPVALUES_INDEX)
    {
        tea_checkapi(index != 0 && -index <= T->top - T->base, "bad stack slot #%d", index);
        return T->top + index;
    }
    else
    {
        GCfunc* func = T->ci->func;
        tea_checkapi(!isteafunc(func), "caller is not a C function");
        index = TEA_UPVALUES_INDEX - index;
        return index < func->c.upvalue_count ? &func->c.upvalues[index] : niltv(T);
    }
}

static TEA_AINLINE TValue* index2addr_check(tea_State* T, int index)
{
    TValue* o = index2addr(T, index);
    tea_checkapi(o != niltv(T), "invalid stack slot #%d", index);
    return o;
}

static TValue* index2addr_stack(tea_State* T, int index)
{
    if(index >= 0)
    {
        TValue* o = T->base + index;
        if(o >= T->top)
        {
            tea_checkapi(0, "invalid stack slot #%d", index);
            return niltv(T);
        }
        else
            return o;
    }
    else
    {
        tea_checkapi(index != 0 && -index <= T->top - T->base, "invalid stack slot #%d", index);
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

/* -- Stack manipulation -------------------------------------------------- */

TEA_API int tea_get_top(tea_State* T)
{
    return (int)(T->top - T->base);
}

TEA_API void tea_set_top(tea_State* T, int index)
{
    if(index >= 0)
    {
        tea_checkapi(index <= T->stack_max - T->base, "bad stack slot #%d", index);
        do { setnilV(T->top++); } while(T->top < T->base + index); 
        T->top = T->base + index;
    }
    else
    {
        tea_checkapi(-(index + 1) <= (T->top - T->base), "bad stack slot #%d", index);
        T->top += index + 1;    /* Shrinks top (index < 0) */
    }
}

TEA_API void tea_remove(tea_State* T, int index)
{
    TValue* p = index2addr_stack(T, index);
    while(++p < T->top) copyTV(T, p - 1, p);
    T->top--;
}

TEA_API void tea_insert(tea_State* T, int index)
{
    TValue* p = index2addr_stack(T, index);
    for(TValue* q = T->top; q > p; q--) copyTV(T, q, q - 1);
    copyTV(T, p, T->top);
}

static void copy_slot(tea_State* T, TValue* f, int index)
{
    TValue* o = index2addr_check(T, index);
    copyTV(T, o, f);
}

TEA_API void tea_replace(tea_State* T, int index)
{
    tea_checkapi_slot(1);
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
    incr_top(T);
}

/* -- Stack getters ------------------------------------------------------ */

TEA_API int tea_get_mask(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(o == niltv(T))
        return TEA_MASK_NONE;
    return 1 << (itype(o) + 1);
}

TEA_API int tea_get_type(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(o == niltv(T))
        return TEA_TYPE_NONE;
    return itype(o) + 1;
}

TEA_API const char* tea_typeof(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    return (o == niltv(T)) ? "no value" : tea_typename(o);
}

TEA_API tea_Number tea_get_number(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    tea_checkapi(tvisnumber(o), "stack slot #%d is not a number", index);
    return numberV(o);
}

TEA_API tea_Integer tea_get_integer(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    tea_checkapi(tvisnumber(o), "stack slot #%d is not a number", index);
    return (tea_Integer)numberV(o);
}

TEA_API bool tea_get_bool(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    tea_checkapi(tvisbool(o), "stack slot #%d is not a bool", index);
    return boolV(o);
}

TEA_API void tea_get_range(tea_State* T, int index, tea_Number* start, tea_Number* end, tea_Number* step)
{
    cTValue* o = index2addr(T, index);
    tea_checkapi(tvisrange(o), "stack slot #%d is not a range", index);
    GCrange* range = rangeV(o);
    if(start) *start = range->start;
    if(end) *end = range->end;
    if(step) *step = range->step;
}

TEA_API const char* tea_get_lstring(tea_State* T, int index, size_t* len)
{
    cTValue* o = index2addr(T, index);
    tea_checkapi(tvisstr(o), "stack slot #%d is not a string", index);
    GCstr* str = strV(o);
    if(len) *len = str->len;
    return str_data(str);
}

TEA_API const char* tea_get_string(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    tea_checkapi(tvisstr(o), "stack slot #%d is not a string", index);
    return str_data(strV(o));
}

TEA_API void* tea_get_userdata(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    tea_checkapi(tvisudata(o), "stack slot #%d is not a userdata", index);
    return ud_data(udataV(o));
}

TEA_API void tea_set_finalizer(tea_State* T, tea_Finalizer f)
{
    tea_checkapi_slot(1);
    cTValue* o = T->top - 1;
    tea_checkapi(tvisudata(o), "stack slot #-1 is not a userdata");
    GCudata* ud = udataV(o);
    ud->fd = f;
}

TEA_API const void* tea_get_pointer(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    tea_checkapi(tvispointer(o), "stack slot #%d is not a pointer", index);
    return pointerV(o);
}

TEA_API bool tea_is_object(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    return tvisgcv(o);
}

TEA_API bool tea_is_cfunction(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    return tvisfunc(o) && !isteafunc(funcV(o));
}

TEA_API bool tea_to_bool(tea_State* T, int index)
{
    TValue* o = index2addr(T, index);
    return (o == niltv(T)) ? false : !tea_obj_isfalse(o);
}

TEA_API tea_Number tea_to_numberx(tea_State* T, int index, bool* is_num)
{
    TValue* o = index2addr(T, index);
    if(o == niltv(T))
    {
        if(is_num) *is_num = false;
        return 0;
    }
    return tea_obj_tonumber(o, is_num);
}

TEA_API tea_Number tea_to_number(tea_State* T, int index)
{
    TValue* o = index2addr(T, index);
    return tea_obj_tonumber(o, NULL);
}

TEA_API tea_Integer tea_to_integerx(tea_State* T, int index, bool* is_num)
{
    TValue* o = index2addr(T, index);
    if(o == niltv(T))
    {
        if(is_num) *is_num = false;
        return 0;
    }
    return (tea_Integer)tea_obj_tonumber(o, is_num);
}

TEA_API tea_Integer tea_to_integer(tea_State* T, int index)
{
    TValue* o = index2addr(T, index);
    return (tea_Integer)tea_obj_tonumber(o, NULL);
}

static GCstr* obj_tostring(tea_State* T, cTValue* o)
{
    if(tvisinstance(o))
    {
        GCinstance* instance = instanceV(o);
        GCstr* _tostring = mmname_str(T, MM_TOSTRING);
        TValue* tostring = tea_tab_get(&instance->klass->methods, _tostring);
        if(tostring)
        {
            setinstanceV(T, T->top++, instance);
            tea_vm_call(T, tostring, 0);
            TValue* o = T->top - 1;
            if(!tvisstr(o))
            {
                tea_err_run(T, TEA_ERR_TOSTR);
            }
            return strV(o);
        }
    }

    SBuf* sb = &T->strbuf;
    tea_buf_reset(sb);
    tea_strfmt_obj(T, sb, o, 0);
    return tea_buf_str(T, sb);
}

TEA_API const char* tea_to_lstring(tea_State* T, int index, size_t* len)
{
    cTValue* o = index2addr(T, index);
    if(o == niltv(T))
    {
        if(len) *len = 0;
        return NULL;
    }
    GCstr* str = obj_tostring(T, o);
    setstrV(T, T->top, str);
    incr_top(T);
    if(len) *len = str->len;
    return str_data(str);
}

TEA_API const char* tea_to_string(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(o == niltv(T))
        return NULL;
    GCstr* str = obj_tostring(T, o);
    setstrV(T, T->top, str);
    incr_top(T);
    return str_data(str);
}

TEA_API tea_CFunction tea_to_cfunction(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    tea_CFunction f = NULL;
    if(tvisfunc(o))
    {
        GCfunc* func = funcV(o);
        if(iscfunc(func))
            f = func->c.fn;
    }
    return f;
}

TEA_API const void* tea_to_pointer(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    return tea_obj_pointer(o);
}

TEA_API void* tea_to_userdata(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(tvisudata(o))
        return ud_data(udataV(o));
    else if(tvispointer(o))
        return pointerV(o);
    else
        return NULL;
}

TEA_API bool tea_equal(tea_State* T, int index1, int index2)
{
    cTValue* o1 = index2addr_check(T, index1);
    cTValue* o2 = index2addr_check(T, index2);
    return tea_obj_equal(o1, o2);
}

TEA_API bool tea_rawequal(tea_State* T, int index1, int index2)
{
    cTValue* o1 = index2addr_check(T, index1);
    cTValue* o2 = index2addr_check(T, index2);
    return tea_obj_rawequal(o1, o2);
}

TEA_API void tea_concat(tea_State* T)
{
    tea_checkapi_slot(2);
    GCstr* s1 = strV(T->top - 2);
    GCstr* s2 = strV(T->top - 1);
    GCstr* str = tea_buf_cat2str(T, s1, s2);
    T->top -= 2;
    setstrV(T, T->top, str);
    incr_top(T);
}

TEA_API void tea_pop(tea_State* T, int n)
{
    T->top -= n;
}

/* -- Stack setters (object creation) -------------------------------------------------- */

TEA_API void tea_push_pointer(tea_State* T, void* p)
{
    setpointerV(T->top, p);
    incr_top(T);
}

TEA_API void tea_push_nil(tea_State* T)
{
    setnilV(T->top);
    incr_top(T);
}

TEA_API void tea_push_true(tea_State* T)
{
    settrueV(T->top);
    incr_top(T);
}

TEA_API void tea_push_false(tea_State* T)
{
    setfalseV(T->top);
    incr_top(T);
}

TEA_API void tea_push_bool(tea_State* T, bool b)
{
    setboolV(T->top, b);
    incr_top(T);
}

TEA_API void tea_push_number(tea_State* T, tea_Number n)
{
    setnumberV(T->top, n);
    incr_top(T);
}

TEA_API void tea_push_integer(tea_State* T, tea_Integer n)
{
    setnumberV(T->top, (tea_Number)n);
    incr_top(T);
}

TEA_API const char* tea_push_lstring(tea_State* T, const char* s, size_t len)
{
    GCstr* str = tea_str_new(T, s, len);
    setstrV(T, T->top, str);
    incr_top(T);
    return str_data(str);
}

TEA_API const char* tea_push_string(tea_State* T, const char* s)
{
    GCstr* str = tea_str_newlen(T, s);
    setstrV(T, T->top, str);
    incr_top(T);
    return str_data(str);
}

TEA_API const char* tea_push_fstring(tea_State* T, const char* fmt, ...)
{
    const char* ret;
    va_list argp;
    va_start(argp, fmt);
    ret = tea_strfmt_pushvf(T, fmt, argp);
    va_end(argp);
    return ret;
}

TEA_API const char* tea_push_vfstring(tea_State* T, const char* fmt, va_list argp)
{
    return tea_strfmt_pushvf(T, fmt, argp);
}

TEA_API void tea_push_range(tea_State* T, tea_Number start, tea_Number end, tea_Number step)
{
    GCrange* range = tea_range_new(T, start, end, step);
    setrangeV(T, T->top, range);
    incr_top(T);
}

TEA_API void tea_new_list(tea_State* T)
{
    GClist* list = tea_list_new(T);
    setlistV(T, T->top, list);
    incr_top(T);
}

TEA_API void tea_new_map(tea_State* T)
{
    GCmap* map = tea_map_new(T);
    setmapV(T, T->top, map);
    incr_top(T);
}

TEA_API void* tea_new_userdata(tea_State* T, size_t size)
{
    GCudata* ud;
    ud = tea_udata_new(T, size);
    setudataV(T, T->top, ud);
    incr_top(T);
    return ud_data(ud);
}

TEA_API void tea_push_cclosure(tea_State* T, tea_CFunction fn, int nargs, int nupvalues)
{
    GCfunc* cf;
    tea_checkapi_slot(nupvalues);
    cf = tea_func_newC(T, C_FUNCTION, fn, nargs, nupvalues);
    T->top -= nupvalues;
    while(nupvalues--)
        copyTV(T, &cf->c.upvalues[nupvalues], T->top + nupvalues);
    setfuncV(T, T->top, cf);
    incr_top(T);
}

TEA_API void tea_push_cfunction(tea_State* T, tea_CFunction fn, int nargs)
{
    GCfunc* cf = tea_func_newC(T, C_FUNCTION, fn, nargs, 0);
    setfuncV(T, T->top, cf);
    incr_top(T);
}

static void set_class(tea_State* T, const tea_Class* k)
{
    for(; k->name != NULL; k++)
    {
        if(k->fn == NULL)
        {
            tea_push_nil(T);
        }
        else
        {
            if(strcmp(k->type, "method") == 0)
            {
                GCfunc* cf = tea_func_newC(T, C_METHOD, k->fn, k->nargs, 0);
                setfuncV(T, T->top++, cf);
            }
            else if(strcmp(k->type, "property") == 0)
            {
                GCfunc* cf = tea_func_newC(T, C_PROPERTY, k->fn, k->nargs, 0);
                setfuncV(T, T->top++, cf);
            }
            else if(strcmp(k->type, "static") == 0)
            {
                GCfunc* cf = tea_func_newC(T, C_FUNCTION, k->fn, k->nargs, 0);
                setfuncV(T, T->top++, cf);
            }
        }
        tea_set_attr(T, -2, k->name);
    }
}

TEA_API void tea_create_class(tea_State* T, const char* name, const tea_Class* klass)
{
    GCclass* k = tea_class_new(T, tea_str_newlen(T, name), NULL);
    setclassV(T, T->top, k);
    incr_top(T);
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
            tea_push_nil(T);
        }
        else
        {
            tea_push_cfunction(T, m->fn, m->nargs);
        }
        tea_set_attr(T, -2, m->name);
    }
}

TEA_API void tea_create_module(tea_State* T, const char* name, const tea_Module* module)
{
    GCstr* modname = tea_str_newlen(T, name);
    GCmodule* mod = tea_module_new(T, modname);
    mod->path = modname;
    setmoduleV(T, T->top, mod);
    incr_top(T);
    if(module != NULL)
    {
        set_module(T, module);
    }
}

/* -- Object getters -------------------------------------------------- */

TEA_API int tea_len(tea_State* T, int index)
{
    cTValue* o = index2addr_check(T, index);
    switch(itype(o))
    {
        case TEA_TSTRING:
        {
            return strV(o)->len;
        }
        case TEA_TLIST:
        {
            return listV(o)->count;
        }
        case TEA_TMAP:
        {
            return mapV(o)->count;
        }
        default:
            break;
    }
    return -1;
}

TEA_API bool tea_get_item(tea_State* T, int list, int index)
{
    cTValue* o = index2addr_check(T, list);
    tea_checkapi(tvislist(o), "stack slot #%d is not a list", list);
    GClist* l = listV(o);
    TValue* items = l->items;
    if(index < 0 || index > l->count - 1)
        return false;
    copyTV(T, T->top, items + index);
    incr_top(T);
    return true;
}

TEA_API bool tea_set_item(tea_State* T, int list, int index)
{
    cTValue* o = index2addr_check(T, list);
    tea_checkapi(tvislist(o), "stack slot #%d is not a list", list);
    tea_checkapi_slot(1);
    GClist* l = listV(o);
    if(index < 0 || index > l->count - 1)
        return false;
    copyTV(T, list_slot(l, index), T->top - 1);
    T->top--;
    return true;
}

TEA_API void tea_add_item(tea_State* T, int list)
{
    cTValue* o = index2addr_check(T, list);
    tea_checkapi(tvislist(o), "stack slot #%d is not a list", list);
    tea_checkapi_slot(1);
    GClist* l = listV(o);
    tea_list_add(T, l, T->top - 1);
    T->top--;
}

TEA_API bool tea_insert_item(tea_State* T, int list, int index)
{
    cTValue* o = index2addr_check(T, list);
    tea_checkapi(tvislist(o), "stack slot #%d is not a list", list);
    tea_checkapi_slot(1);
    GClist* l = listV(o);
    if(index < 0 || index > l->count - 1)
        return false;
    tea_list_insert(T, l, T->top - 1, index);
    T->top--;
    return true;
}

TEA_API bool tea_get_field(tea_State* T, int obj)
{
    cTValue* o = index2addr_check(T, obj);
    tea_checkapi(tvismap(o), "stack slot #%d is not a map", obj);
    tea_checkapi_slot(1);
    TValue* key = T->top - 1;
    bool found = false;

    GCmap* map = mapV(o);
    cTValue* v = tea_map_get(map, key);
    if(v)
    {
        found = true;
        T->top--;
        copyTV(T, T->top, v);
        incr_top(T);
    }
    return found;
}

TEA_API void tea_set_field(tea_State* T, int obj)
{
    cTValue* o = index2addr_check(T, obj);
    tea_checkapi(tvismap(o), "stack slot #%d is not a map", obj);
    tea_checkapi_slot(2);
    TValue* item = T->top - 1;
    TValue* key = T->top - 2;

    GCmap* map = mapV(o);
    copyTV(T, tea_map_set(T, map, key), item);
    T->top -= 2;
}

TEA_API void tea_set_key(tea_State* T, int obj, const char* key)
{
    cTValue* object = index2addr_check(T, obj);
    tea_checkapi(tvismap(object), "stack slot #%d is not a map", obj);
    tea_checkapi_slot(1);
    TValue* item = T->top - 1;
    GCstr* str = tea_str_newlen(T, key);
    GCmap* map = mapV(object);
    copyTV(T, tea_map_setstr(T, map, str), item);
    T->top--;
}

TEA_API bool tea_get_key(tea_State* T, int obj, const char* key)
{
    cTValue* object = index2addr_check(T, obj);
    tea_checkapi(tvismap(object), "stack slot #%d is not a map", obj);
    bool found = false;
    cTValue* o;
    GCmap* map = mapV(object);
    o = tea_map_getstr(T, map, tea_str_newlen(T, key));
    if(o)
    {
        found = true;
        copyTV(T, T->top, o);
        incr_top(T);
    }
    return found;
}

TEA_API bool tea_get_attr(tea_State* T, int obj, const char* key)
{
    cTValue* object = index2addr_check(T, obj);
    bool found = false;
    GCstr* str = tea_str_newlen(T, key);
    cTValue* o;

    switch(itype(object))
    {
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(object);
            o = tea_tab_get(&module->values, str);
            if(o)
            {
                found = true;
                copyTV(T, T->top, o);
                incr_top(T);
            }
            break;
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(object);
            o = tea_map_getstr(T, map, str);
            if(o)
            {
                found = true;
                copyTV(T, T->top, o);
                incr_top(T);
            }
            break;
        }
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(object);
            o = tea_tab_get(&instance->fields, str);
            if(o)
            {
                found = true;
                copyTV(T, T->top, o);
                incr_top(T);
            }
        }
        default:
            break;
    }
    return found;
}

TEA_API void tea_set_attr(tea_State* T, int obj, const char* key)
{
    cTValue* object = index2addr(T, obj);
    tea_checkapi_slot(1);
    TValue* item = T->top - 1;
    GCstr* str = tea_str_newlen(T, key);

    switch(itype(object))
    {
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(object);
            copyTV(T, tea_tab_set(T, &module->values, str, NULL), item);
            break;
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(object);
            copyTV(T, tea_map_setstr(T, map, str), item);
            break;
        }
        case TEA_TCLASS:
        {
            GCclass* klass = classV(object);
            copyTV(T, tea_tab_set(T, &klass->methods, str, NULL), item);
            if(str == T->constructor_string)
            {
                copyTV(T, &klass->constructor, item);
            }
            break;
        }
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(object);
            copyTV(T, tea_tab_set(T, &instance->fields, str, NULL), item);
            break;
        }
        default:
            break;
    }
    T->top--;
}

TEA_API bool tea_get_global(tea_State* T, const char* name)
{
    bool found = false;
    GCstr* str = tea_str_newlen(T, name);
    TValue* o = tea_tab_get(&T->globals, str);
    if(o)
    {
        found = true;
        copyTV(T, T->top, o);
        incr_top(T);
    }
    return found;
}

TEA_API void tea_set_global(tea_State* T, const char* name)
{
    tea_checkapi_slot(1);
    TValue* value = T->top - 1;
    GCstr* str = tea_str_newlen(T, name);
    TValue* o = tea_tab_set(T, &T->globals, str, NULL);
    copyTV(T, o, value);
    T->top--;
}

static void set_globals(tea_State* T, const tea_Reg* reg)
{
    for(; reg->name != NULL; reg++)
    {
        if(reg->fn == NULL)
        {
            tea_push_nil(T);
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

TEA_API void tea_check_type(tea_State* T, int index, int type)
{
    if(tea_get_type(T, index) != type)
        tea_err_argt(T, index, type);
}

TEA_API bool tea_check_bool(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(!tvisbool(o))
        tea_err_argt(T, index, TEA_TYPE_BOOL);
    return boolV(o);
}

TEA_API void tea_check_range(tea_State* T, int index, tea_Number* start, tea_Number* end, tea_Number* step)
{
    cTValue* o = index2addr(T, index);
    if(!tvisrange(o))
        tea_err_argt(T, index, TEA_TYPE_RANGE);
    tea_get_range(T, index, start, end, step);
}

TEA_API void tea_check_any(tea_State* T, int index)
{
    if(index2addr(T, index) == niltv(T))
        tea_err_arg(T, index, TEA_ERR_NOVAL);
}

TEA_API tea_Number tea_check_number(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(!tvisnumber(o))
        tea_err_argt(T, index, TEA_TYPE_NUMBER);
    return numberV(o);
}

TEA_API tea_Integer tea_check_integer(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(!tvisnumber(o))
        tea_err_argt(T, index, TEA_TYPE_NUMBER);
    return (tea_Integer)numberV(o);
}

TEA_API const char* tea_check_lstring(tea_State* T, int index, size_t* len)
{
    cTValue* o = index2addr(T, index);
    if(!tvisstr(o))
        tea_err_argt(T, index, TEA_TYPE_STRING);
    return tea_get_lstring(T, index, len);
}

TEA_API const char* tea_check_string(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(!tvisstr(o))
        tea_err_argt(T, index, TEA_TYPE_STRING);
    return tea_get_string(T, index);
}

TEA_API tea_CFunction tea_check_cfunction(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(!tvisfunc(o) && !iscfunc(funcV(o)))
        tea_err_argtype(T, index, "cfunction");
    return funcV(o)->c.fn;
}

TEA_API void* tea_check_userdata(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(!tvisudata(o))
        tea_err_argt(T, index, TEA_TYPE_USERDATA);
    return ud_data(udataV(o));
}

TEA_API const void* tea_check_pointer(tea_State* T, int index)
{
    cTValue* o = index2addr(T, index);
    if(!tvispointer(o))
        tea_err_argt(T, index, TEA_TYPE_POINTER);
    return pointerV(o);
}

TEA_API void tea_opt_any(tea_State* T, int index)
{
    if(tea_is_none(T, index)) 
        tea_push_nil(T);
}

TEA_API bool tea_opt_bool(tea_State* T, int index, bool def)
{
    return tea_is_nonenil(T, index) ? def : tea_check_bool(T, index);
}

TEA_API tea_Number tea_opt_number(tea_State* T, int index, tea_Number def)
{
    return tea_is_nonenil(T, index) ? def : tea_check_number(T, index);
}

TEA_API tea_Integer tea_opt_integer(tea_State* T, int index, tea_Integer def)
{
    return tea_is_nonenil(T, index) ? def : tea_check_integer(T, index);
}

TEA_API const char* tea_opt_lstring(tea_State* T, int index, const char* def, size_t* len)
{
    if(tea_is_nonenil(T, index))
    {
        if(len) *len = (def ? strlen(def) : 0);
        return def;
    }
    else
        return tea_check_lstring(T, index, len);
}

TEA_API const char* tea_opt_string(tea_State* T, int index, const char* def)
{
    return tea_is_nonenil(T, index) ? def : tea_check_string(T, index);
}

TEA_API const void* tea_opt_pointer(tea_State* T, int index, void* def)
{
    return tea_is_nonenil(T, index) ? def : tea_check_pointer(T, index);
}

TEA_API int tea_check_option(tea_State* T, int index, const char* def, const char* const options[])
{
    const char* name = def ? tea_opt_string(T, index, def) : tea_check_string(T, index);
    for(int i = 0; options[i]; i++)
    {
        if(strcmp(options[i], name) == 0)
            return i;
    }
    tea_error(T, "Invalid option " TEA_QS, name);
    return 0;
}

/* -- GC and memory management -------------------------------------------------- */

TEA_API int tea_gc(tea_State* T)
{
    size_t before = T->gc.total;
    tea_gc_collect(T);
    size_t collected = before - T->gc.total;

    /* GC values are expressed in Kbytes: number of bytes / 2 ** 10 */
    return collected >> 10;
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
    TValue* func;
    tea_checkapi_slot(n + 1);
    func = T->top - n - 1;
    tea_vm_call(T, func, n);
}

TEA_API int tea_pcall(tea_State* T, int n)
{
    CallCtx ctx;
    tea_checkapi_slot(n + 1);
    ctx.func = T->top - n - 1;
    ctx.arg_count = n;
    return tea_vm_pcall(T, call_f, &ctx, stack_save(T, ctx.func));
}

typedef struct CPCallCtx
{
    tea_CFunction func;
    void* ud;
} CPCallCtx;

static void cpcall_f(tea_State* T, void* ud)
{
    CPCallCtx* ctx = (CPCallCtx*)ud;
    GCfunc* fn = tea_func_newC(T, C_FUNCTION, ctx->func, 1, 0);
    setfuncV(T, T->top++, fn);
    setpointerV(T->top++, ctx->ud);
    tea_vm_call(T, T->top - 2, 1);
}

TEA_API int tea_cpcall(tea_State* T, tea_CFunction func, void* ud)
{
    CPCallCtx ctx;
    ctx.func = func;
    ctx.ud = ud;
    return tea_vm_pcall(T, cpcall_f, &ctx, stack_save(T, T->top));
}