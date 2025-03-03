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
#include "tea_buf.h"
#include "tea_tab.h"
#include "tea_list.h"
#include "tea_strfmt.h"
#include "tea_udata.h"
#include "tea_meta.h"
#include "tea_import.h"

/* -- Common helper functions --------------------------------------------- */

#define tea_checkapi_slot(idx) \
    tea_checkapi((idx) <= (T->top - T->base), "stack slot #%d out of range", (idx))

static TValue* index2addr(tea_State* T, int idx)
{
    if(idx >= 0)
    {
        TValue* o = T->base + idx;
        if(o >= T->top)
            return niltv(T);
        else
            return o;
    }
    else if(idx > TEA_REGISTRY_INDEX)
    {
        tea_checkapi(idx != 0 && -idx <= T->top - T->base, "bad stack slot #%d", idx);
        return T->top + idx;
    }
    else if(idx == TEA_REGISTRY_INDEX)
    {
        return registry(T);
    }
    else
    {
        GCfunc* func = T->ci->func;
        tea_checkapi(!isteafunc(func), "caller is not a C function");
        idx = TEA_UPVALUES_INDEX - idx;
        return idx < func->c.upvalue_count ? &func->c.upvalues[idx] : niltv(T);
    }
}

static TEA_AINLINE TValue* index2addr_check(tea_State* T, int idx)
{
    TValue* o = index2addr(T, idx);
    tea_checkapi(o != niltv(T), "invalid stack slot #%d", idx);
    return o;
}

static TValue* index2addr_stack(tea_State* T, int idx)
{
    if(idx >= 0)
    {
        TValue* o = T->base + idx;
        if(o >= T->top)
        {
            tea_checkapi(0, "invalid stack slot #%d", idx);
            return niltv(T);
        }
        else
            return o;
    }
    else
    {
        tea_checkapi(idx != 0 && -idx <= T->top - T->base, "invalid stack slot #%d", idx);
        return T->top + idx;
    }
}

/* -- State manipulation -------------------------------------------------- */

TEA_API void tea_set_argv(tea_State* T, int argc, char** argv, int argf)
{
    T->argc = argc;
    T->argv = argv;
    T->argf = argf;
}

TEA_API int tea_get_argv(tea_State* T, char*** argv, int* argf)
{
    if(argv) *argv = T->argv;
    if(argf) *argf = T->argf;
    return T->argc;
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

#define ispseudo(i) ((i) <= TEA_REGISTRY_INDEX)

TEA_API int tea_absindex(tea_State* T, int idx)
{
    return (idx > 0 || ispseudo(idx)) ? idx : ((int)(T->top - T->base + idx));
}

TEA_API void tea_pop(tea_State* T, int n)
{
    T->top -= n;
}

TEA_API int tea_get_top(tea_State* T)
{
    return (int)(T->top - T->base);
}

TEA_API void tea_set_top(tea_State* T, int idx)
{
    if(idx >= 0)
    {
        tea_checkapi(idx <= T->stack_max - T->base, "bad stack slot #%d", idx);
        do { setnilV(T->top++); } while(T->top < T->base + idx); 
        T->top = T->base + idx;
    }
    else
    {
        tea_checkapi(-(idx + 1) <= (T->top - T->base), "bad stack slot #%d", idx);
        T->top += idx + 1;    /* Shrinks top (idx < 0) */
    }
}

TEA_API void tea_push_value(tea_State* T, int idx)
{
    copyTV(T, T->top, index2addr(T, idx));
    incr_top(T);
}

TEA_API void tea_remove(tea_State* T, int idx)
{
    TValue* p = index2addr_stack(T, idx);
    while(++p < T->top) copyTV(T, p - 1, p);
    T->top--;
}

TEA_API void tea_insert(tea_State* T, int idx)
{
    TValue* p = index2addr_stack(T, idx);
    for(TValue* q = T->top; q > p; q--) copyTV(T, q, q - 1);
    copyTV(T, p, T->top);
}

static void copy_slot(tea_State* T, TValue* f, int idx)
{
    TValue* o = index2addr_check(T, idx);
    copyTV(T, o, f);
}

TEA_API void tea_replace(tea_State* T, int idx)
{
    tea_checkapi_slot(1);
    copy_slot(T, T->top - 1, idx);
    T->top--;
}

TEA_API void tea_copy(tea_State* T, int from_index, int to_index)
{
    copy_slot(T, index2addr(T, from_index), to_index);
}

TEA_API void tea_swap(tea_State* T, int index1, int index2)
{
    TValue tv;
    TValue* o1 = index2addr_check(T, index1);
    TValue* o2 = index2addr_check(T, index2);
    copyTV(T, &tv, o1);
    copyTV(T, o1, o2);
    copyTV(T, o2, &tv);
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

/* -- Stack getters ------------------------------------------------------ */

TEA_API const char* tea_typeof(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    return (o == niltv(T)) ? "no value" : tea_typename(o);
}

TEA_API int tea_get_mask(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(o == niltv(T))
        return TEA_MASK_NONE;
    return 1 << (itype(o) + 1);
}

TEA_API int tea_get_type(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(o == niltv(T))
        return TEA_TYPE_NONE;
    return itype(o) + 1;
}

TEA_API bool tea_get_bool(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    tea_checkapi(tvisbool(o), "stack slot #%d is not a bool", idx);
    return boolV(o);
}

TEA_API tea_Number tea_get_number(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    tea_checkapi(tvisnum(o), "stack slot #%d is not a number", idx);
    return numV(o);
}

TEA_API tea_Integer tea_get_integer(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    tea_checkapi(tvisnum(o), "stack slot #%d is not a number", idx);
    return (tea_Integer)numV(o);
}

TEA_API const void* tea_get_pointer(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    tea_checkapi(tvispointer(o), "stack slot #%d is not a pointer", idx);
    return pointerV(o);
}

TEA_API void tea_get_range(tea_State* T, int idx, tea_Number* start, tea_Number* end, tea_Number* step)
{
    cTValue* o = index2addr(T, idx);
    tea_checkapi(tvisrange(o), "stack slot #%d is not a range", idx);
    GCrange* range = rangeV(o);
    if(start) *start = range->start;
    if(end) *end = range->end;
    if(step) *step = range->step;
}

TEA_API const char* tea_get_lstring(tea_State* T, int idx, size_t* len)
{
    cTValue* o = index2addr(T, idx);
    tea_checkapi(tvisstr(o), "stack slot #%d is not a string", idx);
    GCstr* str = strV(o);
    if(len) *len = str->len;
    return str_data(str);
}

TEA_API const char* tea_get_string(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    tea_checkapi(tvisstr(o), "stack slot #%d is not a string", idx);
    return str_data(strV(o));
}

TEA_API void* tea_get_userdata(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    tea_checkapi(tvisudata(o), "stack slot #%d is not a userdata", idx);
    return ud_data(udataV(o));
}

TEA_API bool tea_get_udvalue(tea_State* T, int ud, int n)
{
    GCudata* udata;
    cTValue* o = index2addr_check(T, ud);
    tea_checkapi(tvisudata(o), "stack slot #%d is not a userdata", ud);
    udata = udataV(o);
    if(n < 0 || n >= udata->nuvals)
        return false;
    copyTV(T, T->top, &ud_uvalues(udata)[n]);
    incr_top(T);
    return true;
}

TEA_API void tea_set_udvalue(tea_State* T, int ud, int n)
{
    GCudata* udata;
    tea_checkapi_slot(1);
    cTValue* o = index2addr_check(T, ud);
    tea_checkapi(tvisudata(o), "stack slot #%d is not a userdata", ud);
    udata = udataV(o);
    if(n < 0 || n >= udata->nuvals)
    {
        tea_checkapi(0, "attempt to index out of bounds uservalue #%d", n);
        return;
    }
    o = T->top - 1;
    TValue* uvalues = ud_uvalues(udata);
    copyTV(T, &uvalues[n], o);
    T->top--;
}

TEA_API bool tea_is_object(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    return tvisgcv(o);
}

TEA_API bool tea_is_cfunction(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    return tvisfunc(o) && !isteafunc(funcV(o));
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

TEA_API bool tea_to_bool(tea_State* T, int idx)
{
    TValue* o = index2addr(T, idx);
    return (o == niltv(T)) ? false : !tea_obj_isfalse(o);
}

TEA_API tea_Number tea_to_numberx(tea_State* T, int idx, bool* is_num)
{
    TValue* o = index2addr(T, idx);
    if(o == niltv(T))
    {
        if(is_num) *is_num = false;
        return 0;
    }
    return tea_obj_tonum(o, is_num);
}

TEA_API tea_Number tea_to_number(tea_State* T, int idx)
{
    TValue* o = index2addr(T, idx);
    return tea_obj_tonum(o, NULL);
}

TEA_API tea_Integer tea_to_integerx(tea_State* T, int idx, bool* is_num)
{
    TValue* o = index2addr(T, idx);
    if(o == niltv(T))
    {
        if(is_num) *is_num = false;
        return 0;
    }
    return (tea_Integer)tea_obj_tonum(o, is_num);
}

TEA_API tea_Integer tea_to_integer(tea_State* T, int idx)
{
    TValue* o = index2addr(T, idx);
    return (tea_Integer)tea_obj_tonum(o, NULL);
}

TEA_API const void* tea_to_pointer(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    return tea_obj_pointer(o);
}

static GCstr* obj_tostring(tea_State* T, cTValue* o)
{
    if(tvisinstance(o) || tvisudata(o))
    {
        TValue* mo = tea_meta_lookup(T, o, MM_TOSTRING);
        if(mo)
        {
            copyTV(T, T->top++, o);
            tea_vm_call(T, mo, 0);
            TValue* o = --T->top;
            if(!tvisstr(o))
                tea_err_msg(T, TEA_ERR_TOSTR);
            return strV(o);
        }
    }
    SBuf* sb = &T->strbuf;
    tea_buf_reset(sb);
    ToStringState st; st.top = 0;
    tea_strfmt_obj(T, sb, o, 0, &st);
    return tea_buf_str(T, sb);
}

TEA_API const char* tea_to_lstring(tea_State* T, int idx, size_t* len)
{
    cTValue* o = index2addr(T, idx);
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

TEA_API const char* tea_to_string(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(o == niltv(T))
        return NULL;
    GCstr* str = obj_tostring(T, o);
    setstrV(T, T->top, str);
    incr_top(T);
    return str_data(str);
}

TEA_API void* tea_to_userdata(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(tvisudata(o))
        return ud_data(udataV(o));
    else if(tvispointer(o))
        return pointerV(o);
    else
        return NULL;
}

TEA_API tea_CFunction tea_to_cfunction(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    tea_CFunction f = NULL;
    if(tvisfunc(o))
    {
        GCfunc* func = funcV(o);
        if(iscfunc(func))
            f = func->c.fn;
    }
    return f;
}

TEA_API void tea_concat(tea_State* T, int n)
{
    if(n >= 2)
    {
        GCstr* str = strV(T->top - n);
        for(int i = 1; i < n; i++)
        {
            GCstr* s = strV(T->top - n + i);
            str = tea_buf_cat2str(T, str, s);
        }
        T->top -= n;
        setstrV(T, T->top, str);
        incr_top(T);
    }
    else if(n == 0)
    {
        /* Push empty string */
        setstrV(T, T->top, &T->strempty);
        incr_top(T);
    }
    /* else n == 1: nothing to do */
}

/* -- Stack setters (object creation) -------------------------------------------------- */

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
    setnumV(T->top, n);
    incr_top(T);
}

TEA_API void tea_push_integer(tea_State* T, tea_Integer n)
{
    setnumV(T->top, (tea_Number)n);
    incr_top(T);
}

TEA_API void tea_push_pointer(tea_State* T, void* p)
{
    setpointerV(T->top, p);
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

TEA_API void tea_push_cclosure(tea_State* T, tea_CFunction fn, int nup, int nargs, int nopts)
{
    GCfunc* cf;
    tea_checkapi_slot(nup);
    cf = tea_func_newC(T, C_FUNCTION, fn, nup, nargs, nopts);
    T->top -= nup;
    while(nup--)
        copyTV(T, &cf->c.upvalues[nup], T->top + nup);
    setfuncV(T, T->top, cf);
    incr_top(T);
}

TEA_API void tea_push_cfunction(tea_State* T, tea_CFunction fn, int nargs, int nopts)
{
    GCfunc* cf = tea_func_newC(T, C_FUNCTION, fn, 0, nargs, nopts);
    setfuncV(T, T->top, cf);
    incr_top(T);
}

TEA_API void tea_new_list(tea_State* T, size_t n)
{
    GClist* list = tea_list_new(T, n);
    setlistV(T, T->top, list);
    incr_top(T);
}

TEA_API void tea_new_map(tea_State* T)
{
    GCmap* map = tea_map_new(T);
    setmapV(T, T->top, map);
    incr_top(T);
}

TEA_API void tea_new_class(tea_State* T, const char* name)
{
    GCclass* klass = tea_class_new(T, tea_str_newlen(T, name));
    setclassV(T, T->top, klass);
    incr_top(T);
}

TEA_API void tea_new_module(tea_State* T, const char* name)
{
    GCstr* modname = tea_str_newlen(T, name);
    GCmodule* mod = tea_module_new(T, modname);
    mod->path = modname;
    setmoduleV(T, T->top, mod);
    incr_top(T);
}

TEA_API void tea_new_submodule(tea_State* T, const char* name)
{
    GCstr* modname = tea_str_newlen(T, name);
    GCmodule* mod = tea_submodule_new(T, modname);
    mod->path = modname;
    setmoduleV(T, T->top, mod);
    incr_top(T);
}

static void set_method(tea_State* T, int obj, const char* name, uint8_t flags)
{
    TValue* object = index2addr(T, obj);
    tea_checkapi_slot(1);
    TValue* item = T->top - 1;
    GCstr* str = tea_str_newlen(T, name);
    GCclass* k = classV(object);
    copyTV(T, tea_tab_setx(T, &k->methods, str, flags), item);
    T->top--;
    if(str == mmname_str(T, MM_NEW))
    {
        copyTV(T, &k->init, item);
    }
}

static void set_ctype(tea_State* T, const char* name, CFuncType* ct, uint8_t* flags)
{
    if(strcmp(name, "method") == 0)
    {
        *ct = C_METHOD;
    }
    else if(strcmp(name, "static") == 0)
    {
        *ct = C_FUNCTION;
        *flags = ACC_STATIC;
    }
    else if(strcmp(name, "getter") == 0)
    {
        *ct = C_PROPERTY;
        *flags = ACC_GET;
    }
    else if(strcmp(name, "setter") == 0)
    {
        *ct = C_PROPERTY;
        *flags = ACC_SET;
    }
    else
    {
        tea_error(T, "Invalid option " TEA_QS, name);
    }
}

static void set_class(tea_State* T, const tea_Methods* k)
{
    for(; k->name; k++)
    {
        uint8_t flags = 0;
        if(k->fn == NULL)
        {
            tea_push_nil(T);
        }
        else
        {
            CFuncType ct = C_FUNCTION;
            set_ctype(T, k->type, &ct, &flags);
            GCfunc* cf = tea_func_newC(T, ct, k->fn, 0, k->nargs, k->nopts);
            setfuncV(T, T->top, cf);
            incr_top(T);
        }
        set_method(T, -2, k->name, flags);
    }
}

TEA_API void tea_create_class(tea_State* T, const char* name, const tea_Methods* klass)
{
    GCclass* k = tea_class_new(T, tea_str_newlen(T, name));
    setclassV(T, T->top, k);
    incr_top(T);
    if(klass)
    {
        set_class(T, klass);
    }
}

static void set_module(tea_State* T, const tea_Reg* m)
{
    for(; m->name; m++)
    {
        if(m->fn == NULL)
        {
            tea_push_nil(T);
        }
        else
        {
            tea_push_cfunction(T, m->fn, m->nargs, m->nopts);
        }
        tea_set_attr(T, -2, m->name);
    }
}

TEA_API void tea_create_module(tea_State* T, const char* name, const tea_Reg* module)
{
    GCstr* modname = tea_str_newlen(T, name);
    GCmodule* mod = tea_module_new(T, modname);
    mod->path = modname;
    setmoduleV(T, T->top, mod);
    incr_top(T);
    if(module)
    {
        set_module(T, module);
    }
}

TEA_API void tea_create_submodule(tea_State* T, const char* name, const tea_Reg* module)
{
    GCstr* modname = tea_str_newlen(T, name);
    GCmodule* mod = tea_submodule_new(T, modname);
    mod->path = modname;
    setmoduleV(T, T->top, mod);
    incr_top(T);
    if(module)
    {
        set_module(T, module);
    }
}

TEA_API void* tea_new_userdatav(tea_State* T, size_t size, int nuvs)
{
    GCudata* ud;
    ud = tea_udata_new(T, size, (uint8_t)nuvs);
    setudataV(T, T->top, ud);
    incr_top(T);
    return ud_data(ud);
}

TEA_API void* tea_new_udatav(tea_State* T, size_t size, int nuvs, const char* name)
{
    GCclass* klass = classV(tea_map_getstr(T, mapV(registry(T)), tea_str_newlen(T, name)));
    GCudata* ud = tea_udata_new(T, size, (uint8_t)nuvs);
    ud->klass = klass;
    setudataV(T, T->top, ud);
    incr_top(T);
    return ud_data(ud);
}

TEA_API void* tea_new_userdata(tea_State* T, size_t size)
{
    GCudata* ud;
    ud = tea_udata_new(T, size, 0);
    setudataV(T, T->top, ud);
    incr_top(T);
    return ud_data(ud);
}

TEA_API void* tea_new_udata(tea_State* T, size_t size, const char* name)
{
    GCclass* klass = classV(tea_map_getstr(T, mapV(registry(T)), tea_str_newlen(T, name)));
    GCudata* ud = tea_udata_new(T, size, 0);
    ud->klass = klass;
    setudataV(T, T->top, ud);
    incr_top(T);
    return ud_data(ud);
}

/* -- Object getters -------------------------------------------------- */

TEA_API int tea_len(tea_State* T, int idx)
{
    cTValue* o = index2addr_check(T, idx);
    switch(itype(o))
    {
        case TEA_TSTR:
        {
            return strV(o)->len;
        }
        case TEA_TLIST:
        {
            return listV(o)->len;
        }
        case TEA_TMAP:
        {
            return mapV(o)->count;
        }
        case TEA_TUDATA:
        {
            return udataV(o)->len;
        }
        default:
            break;
    }
    return -1;
}

TEA_API int tea_next(tea_State* T, int obj)
{
    TValue* o = index2addr(T, obj);
    int more;
    tea_checkapi(tvismap(o), "stack slot #%d is not a map", obj);
    more = tea_map_next(mapV(o), T->top - 1, T->top - 1);
    if(more > 0)
    {
        /* Return new key and value slot */
        incr_top(T);
    }
    else if(!more)
    {
        /* Remove key slot */
        T->top--;
    }
    else
    {
        tea_err_msg(T, TEA_ERR_NEXTIDX);
    }
    return more;
}

TEA_API bool tea_get_item(tea_State* T, int list, int idx)
{
    cTValue* o = index2addr_check(T, list);
    tea_checkapi(tvislist(o), "stack slot #%d is not a list", list);
    GClist* l = listV(o);
    TValue* items = l->items;
    if(idx < 0 || idx > l->len - 1)
        return false;
    copyTV(T, T->top, items + idx);
    incr_top(T);
    return true;
}

TEA_API bool tea_set_item(tea_State* T, int list, int idx)
{
    cTValue* o = index2addr_check(T, list);
    tea_checkapi(tvislist(o), "stack slot #%d is not a list", list);
    tea_checkapi_slot(1);
    GClist* l = listV(o);
    if(idx < 0 || idx > l->len - 1)
        return false;
    copyTV(T, list_slot(l, idx), T->top - 1);
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

TEA_API bool tea_insert_item(tea_State* T, int list, int idx)
{
    cTValue* o = index2addr_check(T, list);
    tea_checkapi(tvislist(o), "stack slot #%d is not a list", list);
    tea_checkapi_slot(1);
    GClist* l = listV(o);
    if(idx < 0 || idx > l->len - 1)
        return false;
    tea_list_insert(T, l, T->top - 1, idx);
    T->top--;
    return true;
}

TEA_API bool tea_delete_item(tea_State* T, int list, int idx)
{
    cTValue* o = index2addr_check(T, list);
    tea_checkapi(tvislist(o), "stack slot #%d is not a list", list);
    GClist* l = listV(o);
    if(idx < 0 || idx > l->len - 1)
        return false;
    tea_list_delete(T, l, idx);
    T->top--;
    return true;
}

TEA_API bool tea_get_fieldi(tea_State* T, int obj, tea_Integer i)
{
    cTValue* o = index2addr_check(T, obj);
    tea_checkapi(tvismap(o), "stack slot #%d is not a map", obj);
    tea_checkapi_slot(1);
    bool found = false;
    TValue key;
    setintV(&key, i);

    GCmap* map = mapV(o);
    cTValue* v = tea_map_get(map, &key);
    if(v)
    {
        found = true;
        copyTV(T, T->top, v);
        incr_top(T);
    }
    return found;
}

TEA_API void tea_set_fieldi(tea_State* T, int obj, tea_Integer i)
{
    cTValue* o = index2addr_check(T, obj);
    tea_checkapi(tvismap(o), "stack slot #%d is not a map", obj);
    tea_checkapi_slot(1);
    TValue* item = T->top - 1;
    TValue key;
    setintV(&key, i);

    GCmap* map = mapV(o);
    copyTV(T, tea_map_set(T, map, &key), item);
    T->top--;
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

TEA_API bool tea_delete_field(tea_State* T, int obj)
{
    cTValue* o = index2addr_check(T, obj);
    tea_checkapi(tvismap(o), "stack slot #%d is not a map", obj);
    tea_checkapi_slot(2);
    TValue* key = T->top - 1;

    GCmap* map = mapV(o);
    bool found = tea_map_delete(T, map, key);
    T->top--;
    return found;
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

TEA_API bool tea_delete_key(tea_State* T, int obj, const char* key)
{
    cTValue* object = index2addr_check(T, obj);
    tea_checkapi(tvismap(object), "stack slot #%d is not a map", obj);
    tea_checkapi_slot(1);
    GCmap* map = mapV(object);
    TValue o;
    setstrV(T, &o, tea_str_newlen(T, key));
    return tea_map_delete(T, map, &o);
}

TEA_API bool tea_has_attr(tea_State* T, int obj, const char* key)
{
    TValue* object = index2addr_check(T, obj);
    GCstr* str = tea_str_newlen(T, key);
    return tea_meta_hasattr(T, str, object);
}

TEA_API void tea_get_attr(tea_State* T, int obj, const char* key)
{
    TValue* object = index2addr_check(T, obj);
    GCstr* str = tea_str_newlen(T, key);
    cTValue* o = tea_meta_getattr(T, str, object);
    copyTV(T, T->top, o);
    incr_top(T);
}

TEA_API void tea_set_attr(tea_State* T, int obj, const char* key)
{
    TValue* object = index2addr(T, obj);
    tea_checkapi_slot(1);
    TValue* item = T->top - 1;
    GCstr* str = tea_str_newlen(T, key);
    tea_meta_setattr(T, str, object, item);
    T->top--;
}

TEA_API void tea_delete_attr(tea_State* T, int obj, const char* key)
{
    TValue* object = index2addr(T, obj);
    GCstr* str = tea_str_newlen(T, key);
    tea_meta_delattr(T, str, object);
}

TEA_API void tea_get_index(tea_State* T, int obj)
{
    TValue* o = index2addr_check(T, obj);
    tea_checkapi_slot(1);
    TValue* key = T->top - 1;
    cTValue* v = tea_meta_getindex(T, o, key);
    T->top--;
    copyTV(T, T->top, v);
    incr_top(T);
}

TEA_API void tea_set_index(tea_State* T, int obj)
{
    TValue* o = index2addr_check(T, obj);
    tea_checkapi_slot(2);
    TValue* item = T->top - 1;
    TValue* key = T->top - 2;
    tea_meta_setindex(T, o, key, item);
    T->top -= 2;
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
    TValue* o = T->top - 1;
    GCstr* str = tea_str_newlen(T, name);
    copyTV(T, tea_tab_set(T, &T->globals, str), o);
    T->top--;
}

TEA_API bool tea_get_var(tea_State* T, const char* name, const char* var)
{
    bool found = false;
    GCstr* s1 = tea_str_newlen(T, name);
    GCstr* s2 = tea_str_newlen(T, var);
    GCmodule* module = moduleV(tea_tab_get(&T->modules, s1));
    TValue* o = tea_tab_get(&module->exports, s2);
    if(o)
    {
        found = true;
        copyTV(T, T->top, o);
        incr_top(T);
    }
    return found;
}

TEA_API void tea_set_var(tea_State* T, const char* name, const char* var)
{
    tea_checkapi_slot(1);
    TValue* o = T->top - 1;
    GCstr* s1 = tea_str_newlen(T, name);
    GCstr* s2 = tea_str_newlen(T, var);
    GCmodule* module = moduleV(tea_tab_get(&T->modules, s1));
    copyTV(T, tea_tab_set(T, &module->exports, s2), o);
    T->top--;
}

TEA_API void tea_set_funcs(tea_State* T, const tea_Reg* reg, int nup)
{
    tea_check_stack(T, nup, "too many upvalues");
    for(; reg->name; reg++)
    {
        int i;
        for(i = 0; i < nup; i++)  /* Copy upvalues to the top */
            tea_push_value(T, -nup);
        tea_push_cclosure(T, reg->fn, nup, reg->nargs, reg->nopts);
        tea_set_attr(T, -(nup + 2), reg->name);
    }
    tea_pop(T, nup);    /* Remove upvalues */
}

TEA_API void tea_set_methods(tea_State* T, const tea_Methods* reg, int nup)
{
    tea_check_stack(T, nup, "too many upvalues");
    for(; reg->name; reg++)
    {
        uint8_t flags = 0;
        if(reg->fn == NULL)
        {
            tea_push_nil(T);
        }
        else
        {
            CFuncType ct = C_FUNCTION;
            set_ctype(T, reg->type, &ct, &flags);
            GCfunc* cf = tea_func_newC(T, ct, reg->fn, nup, reg->nargs, reg->nopts);
            int nupvals = nup;
            while(nupvals--)
                copyTV(T, &cf->c.upvalues[nupvals], T->top + nupvals);
            setfuncV(T, T->top, cf);
            incr_top(T);
        }
        set_method(T, -(nup + 2), reg->name, flags);
    }
    tea_pop(T, nup);    /* Remove upvalues */
}

TEA_API void tea_check_type(tea_State* T, int idx, int type)
{
    if(tea_get_type(T, idx) != type)
        tea_err_argt(T, idx, type);
}

TEA_API void tea_check_any(tea_State* T, int idx)
{
    if(index2addr(T, idx) == niltv(T))
        tea_err_arg(T, idx, TEA_ERR_NOVAL);
}

TEA_API bool tea_check_bool(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(!tvisbool(o))
        tea_err_argt(T, idx, TEA_TYPE_BOOL);
    return boolV(o);
}

TEA_API tea_Number tea_check_number(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(!tvisnum(o))
        tea_err_argt(T, idx, TEA_TYPE_NUMBER);
    return numV(o);
}

TEA_API tea_Integer tea_check_integer(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(!tvisnum(o))
        tea_err_argt(T, idx, TEA_TYPE_NUMBER);
    return (tea_Integer)numV(o);
}

TEA_API const void* tea_check_pointer(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(!tvispointer(o))
        tea_err_argt(T, idx, TEA_TYPE_POINTER);
    return pointerV(o);
}

TEA_API void tea_check_range(tea_State* T, int idx, tea_Number* start, tea_Number* end, tea_Number* step)
{
    cTValue* o = index2addr(T, idx);
    if(!tvisrange(o))
        tea_err_argt(T, idx, TEA_TYPE_RANGE);
    tea_get_range(T, idx, start, end, step);
}

TEA_API const char* tea_check_lstring(tea_State* T, int idx, size_t* len)
{
    cTValue* o = index2addr(T, idx);
    if(!tvisstr(o))
        tea_err_argt(T, idx, TEA_TYPE_STRING);
    return tea_get_lstring(T, idx, len);
}

TEA_API const char* tea_check_string(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(!tvisstr(o))
        tea_err_argt(T, idx, TEA_TYPE_STRING);
    return tea_get_string(T, idx);
}

TEA_API tea_CFunction tea_check_cfunction(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(!tvisfunc(o) && !iscfunc(funcV(o)))
        tea_err_argtype(T, idx, "cfunction");
    return funcV(o)->c.fn;
}

TEA_API void* tea_check_userdata(tea_State* T, int idx)
{
    cTValue* o = index2addr(T, idx);
    if(!tvisudata(o))
        tea_err_argt(T, idx, TEA_TYPE_USERDATA);
    return ud_data(udataV(o));
}

TEA_API void* tea_test_udata(tea_State* T, int idx, const char* name)
{
    cTValue* o = index2addr(T, idx);
    if(tvisudata(o))
    {
        GCudata* ud = udataV(o);
        cTValue* tv = tea_map_getstr(T, mapV(registry(T)), tea_str_newlen(T, name));
        if(tv && tvisclass(tv) && classV(tv) == ud->klass)
            return ud_data(ud);
    }
    return NULL;
}

TEA_API void* tea_check_udata(tea_State* T, int idx, const char* name)
{
    void* p = tea_test_udata(T, idx, name);
    if(!p) tea_err_argtype(T, idx, name);
    return p;
}

TEA_API int tea_check_option(tea_State* T, int idx, const char* def, const char* const options[])
{
    const char* name = def ? tea_opt_string(T, idx, def) : tea_check_string(T, idx);
    for(int i = 0; options[i]; i++)
    {
        if(strcmp(options[i], name) == 0)
            return i;
    }
    tea_error(T, "Invalid option " TEA_QS, name);
    return 0;
}

TEA_API void tea_opt_nil(tea_State* T, int idx)
{
    if(tea_is_none(T, idx)) 
        tea_push_nil(T);
}

TEA_API bool tea_opt_bool(tea_State* T, int idx, bool def)
{
    return tea_is_nonenil(T, idx) ? def : tea_check_bool(T, idx);
}

TEA_API tea_Number tea_opt_number(tea_State* T, int idx, tea_Number def)
{
    return tea_is_nonenil(T, idx) ? def : tea_check_number(T, idx);
}

TEA_API tea_Integer tea_opt_integer(tea_State* T, int idx, tea_Integer def)
{
    return tea_is_nonenil(T, idx) ? def : tea_check_integer(T, idx);
}

TEA_API const void* tea_opt_pointer(tea_State* T, int idx, void* def)
{
    return tea_is_nonenil(T, idx) ? def : tea_check_pointer(T, idx);
}

TEA_API const char* tea_opt_lstring(tea_State* T, int idx, const char* def, size_t* len)
{
    if(tea_is_nonenil(T, idx))
    {
        if(len) *len = (def ? strlen(def) : 0);
        return def;
    }
    else
        return tea_check_lstring(T, idx, len);
}

TEA_API const char* tea_opt_string(tea_State* T, int idx, const char* def)
{
    return tea_is_nonenil(T, idx) ? def : tea_check_string(T, idx);
}

TEA_API void* tea_opt_userdata(tea_State* T, int idx, void* def)
{
    return tea_is_nonenil(T, idx) ? def : tea_check_userdata(T, idx);
}

TEA_API tea_CFunction tea_opt_cfunction(tea_State* T, int idx, tea_CFunction def)
{
    return tea_is_nonenil(T, idx) ? def : tea_check_cfunction(T, idx);
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

TEA_API void* tea_alloc(tea_State* T, size_t size)
{
    return tea_mem_new(T, size);
}

TEA_API void* tea_realloc(tea_State* T, void* p, size_t size)
{
    return tea_mem_realloc(T, p, 0, size);
}

TEA_API void tea_free(tea_State* T, void* p)
{
    tea_mem_free(T, p, 0);
}

TEA_API void tea_set_finalizer(tea_State* T, tea_Finalizer f)
{
    tea_checkapi_slot(1);
    cTValue* o = T->top - 1;
    tea_checkapi(tvisudata(o), "stack slot #-1 is not a userdata");
    GCudata* ud = udataV(o);
    ud->fd = f;
}

/* -- Calls -------------------------------------------------- */

typedef struct CallCtx
{
    TValue* func;
    int nargs;
} CallCtx;

static void call_f(tea_State* T, void* ud)
{
    CallCtx* ctx = (CallCtx*)ud;
    tea_vm_call(T, ctx->func, ctx->nargs);
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
    ctx.nargs = n;
    return tea_vm_pcall(T, call_f, &ctx, stack_save(T, ctx.func));
}

TEA_API int tea_pccall(tea_State* T, tea_CFunction func, void* ud)
{
    CallCtx ctx;
    GCfunc* fn = tea_func_newC(T, C_FUNCTION, func, 0, 1, 0);
    setfuncV(T, T->top++, fn);
    setpointerV(T->top++, ud);
    ctx.func = T->top - 2;
    ctx.nargs = 1;
    return tea_vm_pcall(T, call_f, &ctx, stack_save(T, ctx.func));
}

TEA_API void tea_import(tea_State* T, const char* name)
{
    GCstr* s = tea_str_newlen(T, name);
    tea_imp_logical(T, s);
}