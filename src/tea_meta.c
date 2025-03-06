/*
** tea_meta.c
** Method handling
*/

#define tea_meta_c
#define TEA_CORE

#include "tea_tab.h"
#include "tea_str.h"
#include "tea_gc.h"
#include "tea_meta.h"
#include "tea_err.h"
#include "tea_map.h"
#include "tea_list.h"
#include "tea_vm.h"
#include "tea_utf.h"

/* String interning of special method names for fast indexing */
void tea_meta_init(tea_State* T)
{
    const char* const mmnames[] = {
#define MMSTR(_, name) #name,
        MMDEF(MMSTR)
#undef MMSTR
    };
    for(int i = 0; i < MM__MAX; i++)
    {
        GCstr* s = tea_str_newlen(T, mmnames[i]);
        fix_string(s);
        T->gcroot[GCROOT_MMNAME + i] = obj2gco(s);
    }
}

/* Lookup method for object */
TValue* tea_meta_lookup(tea_State* T, cTValue* o, MMS mm)
{
    GCclass* klass = NULL;
    if(tvisinstance(o))
        klass = instanceV(o)->klass;
    else if(tvisudata(o))
        klass = udataV(o)->klass;
    else
        klass = tea_meta_getclass(T, o);
    if(klass)
    {
        TValue* mo = tea_tab_get(&klass->methods, mmname_str(T, mm));
        if(mo)
            return mo;
    }
    return NULL;
}

/* Get builtin class from object value */
GCclass* tea_meta_getclass(tea_State* T, cTValue* o)
{
    switch(itype(o))
    {
        case TEA_TNUM:
            return gcroot_numclass(T);
        case TEA_TBOOL:
            return gcroot_boolclass(T);
        case TEA_TFUNC:
            return gcroot_funcclass(T);
        case TEA_TUDATA:
        case TEA_TINSTANCE:
            return instanceV(o)->klass;
        case TEA_TLIST: 
            return gcroot_listclass(T);
        case TEA_TMAP: 
            return gcroot_mapclass(T);
        case TEA_TSTR: 
            return gcroot_strclass(T);
        case TEA_TRANGE: 
            return gcroot_rangeclass(T);
        default: 
            break;
    }
    return NULL;
}

/* Check if object is a builtin class */
bool tea_meta_isclass(tea_State* T, GCclass* klass)
{
    return (klass == gcroot_numclass(T) ||
            klass == gcroot_boolclass(T) ||
            klass == gcroot_funcclass(T) ||
            klass == gcroot_listclass(T) ||
            klass == gcroot_mapclass(T) ||
            klass == gcroot_strclass(T) ||
            klass == gcroot_rangeclass(T));
}

/* Check if object has attribute */
bool tea_meta_hasattr(tea_State* T, GCstr* name, TValue* obj)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            cTValue* o = tea_tab_get(&instance->attrs, name);
            if(o) return true;
            break;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(obj);
            cTValue* o = tea_tab_get(&module->exports, name);
            if(o) return true;
            break;
        }
        default:
            break;
    }
    return false;
}

/* Get attribute from object */ 
cTValue* tea_meta_getattr(tea_State* T, GCstr* name, TValue* obj)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            uint8_t flags = ACC_GET;
            TValue* mo = tea_tab_get(&instance->attrs, name);
            if(mo) return mo;
            mo = tea_tab_getx(&instance->klass->methods, name, &flags);
            if(mo)
            {
                if(flags & ACC_GET)
                {
                    copyTV(T, T->top++, obj);
                    tea_vm_call(T, mo, 0);
                    return --T->top;
                }
                GCmethod* bound = tea_method_new(T, T->top - 1, funcV(mo));
                setmethodV(T, &T->tmptv, bound);
                return &T->tmptv;
            }
            mo = tea_meta_lookup(T, obj, MM_GETATTR);
            if(mo)
            {
                copyTV(T, T->top++, obj);
                setstrV(T, T->top++, name);
                tea_vm_call(T, mo, 1);
                return --T->top;
            }
            tea_err_callerv(T, TEA_ERR_METHOD, str_data(name));
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(obj);
            cTValue* o = tea_tab_get(&module->exports, name);
            if(o) return o;
            tea_err_callerv(T, TEA_ERR_MODATTR, str_data(module->name), str_data(name));
        }
        default:
        {
            GCclass* klass = tea_meta_getclass(T, obj);
            if(klass)
            {
                uint8_t flags = ACC_GET;
                TValue* mo = tea_tab_getx(&klass->methods, name, &flags);
                if(mo)
                {
                    if(flags & ACC_GET)
                    {
                        copyTV(T, T->top++, obj);
                        tea_vm_call(T, mo, 0);
                        return --T->top;
                    }
                    GCmethod* bound = tea_method_new(T, T->top - 1, funcV(mo));
                    setmethodV(T, &T->tmptv, bound);
                    return &T->tmptv;
                }
            }
            break;
        }
    }
    tea_err_callerv(T, TEA_ERR_NOATTR, tea_typename(obj), str_data(name));
}

/* Set attribute to object */
cTValue* tea_meta_setattr(tea_State* T, GCstr* name, TValue* obj, TValue* item)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            uint8_t flags = ACC_SET;
            TValue* mo = tea_tab_getx(&instance->klass->methods, name, &flags);
            if(mo && (flags & ACC_SET))
            {
                copyTV(T, T->top++, obj);
                copyTV(T, T->top++, item);
                tea_vm_call(T, mo, 1);
                return --T->top;
            }
            mo = tea_meta_lookup(T, obj, MM_SETATTR);
            if(mo)
            {             
                copyTV(T, T->top++, obj);
                setstrV(T, T->top++, name);
                copyTV(T, T->top++, item);
                tea_vm_call(T, mo, 2);
                return --T->top;
            }
            copyTV(T, tea_tab_set(T, &instance->attrs, name), item);
            return item;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(obj);
            copyTV(T, tea_tab_set(T, &module->exports, name), item);
            return item;
        }
        default:
        {
            GCclass* klass = tea_meta_getclass(T, obj);
            if(klass)
            {
                uint8_t flags = ACC_SET;
                TValue* mo = tea_tab_getx(&klass->methods, name, &flags);
                if(mo && (flags & ACC_SET))
                {
                    copyTV(T, T->top++, obj);
                    copyTV(T, T->top++, item);
                    tea_vm_call(T, mo, 1);
                    return --T->top;
                }
            }
            break;
        }
    }
    tea_err_callerv(T, TEA_ERR_SETATTR, tea_typename(obj));
}

/* Delete attribute from object */
void tea_meta_delattr(tea_State* T, GCstr* name, TValue* obj)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            if(!tea_tab_delete(&instance->attrs, name))
                break;
            return;
        }
        case TEA_TMODULE:
        {
            GCmodule* module = moduleV(obj);
            if(!tea_tab_delete(&module->exports, name))
                break;
            return;
        }
        default:
            break;
    }
    tea_err_callerv(T, TEA_ERR_NOATTR, tea_typename(obj), str_data(name));
}

/* Get index from object */
cTValue* tea_meta_getindex(tea_State* T, TValue* obj, TValue* index_value)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            TValue* mo = tea_meta_lookup(T, obj, MM_GETINDEX);
            if(mo)
            {
                copyTV(T, T->top++, obj);
                copyTV(T, T->top++, index_value);
                tea_vm_call(T, mo, 1);
                return --T->top;
            }
            tea_err_callerv(T, TEA_ERR_INSTSUBSCR, str_data(instance->klass->name));
        }
        case TEA_TRANGE:
        {
            if(TEA_UNLIKELY(!tvisnum(index_value)))
            {
                tea_err_msg(T, TEA_ERR_NUMRANGE);
            }

            GCrange* range = rangeV(obj);
            double idx = numV(index_value);

            /* Calculate the length of the range */
            double len = (range->end - range->start) / range->step;

            /* Allow negative indexes */
            if(idx < 0)
            {
                idx = len + idx;
            }

            if(idx >= 0 && idx < len)
            {
                setnumV(&T->tmptv, range->start + idx * range->step);
                return &T->tmptv;
            }
            tea_err_msg(T, TEA_ERR_IDXRANGE);
        }
        case TEA_TLIST:
        {
            if(TEA_UNLIKELY(!tvisnum(index_value) && !tvisrange(index_value)))
            {
                tea_err_msg(T, TEA_ERR_NUMLIST);
            }

            if(tvisrange(index_value))
            {
                GClist* l = tea_list_slice(T, listV(obj), rangeV(index_value));
                setlistV(T, &T->tmptv, l);
                return &T->tmptv;
            }

            GClist* list = listV(obj);
            int32_t idx = numV(index_value);

            /* Allow negative indexes */
            if(idx < 0)
            {
                idx = list->len + idx;
            }

            if(idx >= 0 && idx < list->len)
            {
                return list_slot(list, idx);
            }
            tea_err_msg(T, TEA_ERR_IDXLIST);
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            cTValue* o = tea_map_get(map, index_value);
            if(o) return o;
            tea_err_msg(T, TEA_ERR_MAPKEY);
        }
        case TEA_TSTR:
        {
            if(TEA_UNLIKELY(!tvisnum(index_value) && !tvisrange(index_value)))
            {
                tea_err_callerv(T, TEA_ERR_NUMSTR, tea_typename(index_value));
            }

            if(tvisrange(index_value))
            {
                GCstr* s = tea_utf_slice(T, strV(obj), rangeV(index_value));
                setstrV(T, &T->tmptv, s);
                return &T->tmptv;
            }

            GCstr* str = strV(obj);
            int32_t idx = numV(index_value);
            int32_t ulen = tea_utf_len(str);

            /* Allow negative indexes */
            if(idx < 0)
            {
                idx = ulen + idx;
            }

            if(idx >= 0 && idx < ulen)
            {
                GCstr* c = tea_utf_codepoint_at(T, str, tea_utf_char_offset(str_datawr(str), idx));
                setstrV(T, &T->tmptv, c);
                return &T->tmptv;
            }
            tea_err_msg(T, TEA_ERR_IDXSTR);
        }
        default:
            break;
    }
    tea_err_callerv(T, TEA_ERR_SUBSCR, tea_typename(obj));
}

/* Set index to object */
cTValue* tea_meta_setindex(tea_State* T, TValue* obj, TValue* index_value, TValue* item_value)
{
    switch(itype(obj))
    {
        case TEA_TUDATA:
        case TEA_TINSTANCE:
        {
            GCinstance* instance = instanceV(obj);
            TValue* mo = tea_meta_lookup(T, obj, MM_SETINDEX);
            if(mo)
            {             
                copyTV(T, T->top++, obj);
                copyTV(T, T->top++, index_value);
                copyTV(T, T->top++, item_value);
                tea_vm_call(T, mo, 2);
                return --T->top;
            }
            tea_err_callerv(T, TEA_ERR_SETSUBSCR, instance->klass->name);
        }
        case TEA_TLIST:
        {
            if(TEA_UNLIKELY(!tvisnum(index_value)))
            {
                tea_err_msg(T, TEA_ERR_NUMLIST);
            }

            GClist* list = listV(obj);
            int32_t idx = numV(index_value);

            if(idx < 0)
            {
                idx = list->len + idx;
            }

            if(idx >= 0 && idx < list->len)
            {
                copyTV(T, list_slot(list, idx), item_value);
                return item_value;
            }
            tea_err_msg(T, TEA_ERR_IDXLIST);
        }
        case TEA_TMAP:
        {
            GCmap* map = mapV(obj);
            copyTV(T, tea_map_set(T, map, index_value), item_value);
            return item_value;
        }
        default:
            break;
    }
    tea_err_callerv(T, TEA_ERR_SETSUBSCR, tea_typename(obj));
}