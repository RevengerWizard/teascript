/*
** tea_obj.c
** Teascript values and objects
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define tea_obj_c
#define TEA_CORE

#include "tea_def.h"
#include "tea_gc.h"
#include "tea_obj.h"
#include "tea_map.h"
#include "tea_tab.h"
#include "tea_strscan.h"

/* Object type names */
TEA_DATADEF const char* const tea_obj_typenames[] = {
    "nil", "bool", "number", "pointer", 
    "string", "range", "function",
    "module", "class", "instance", "list", "map",
    "userdata", "proto", "upvalue", "method"
};

GCmodule* tea_module_new(tea_State* T, GCstr* name)
{
    char c = str_data(name)[0];
    TValue* module_val = tea_tab_get(&T->modules, name);
    if(c != '?' && module_val)
    {
        return moduleV(module_val);
    }

    GCmodule* module = tea_mem_newobj(T, GCmodule, TEA_TMODULE);
    tea_tab_init(&module->vars);
    tea_tab_init(&module->exports);
    module->name = name;
    module->path = NULL;

    setmoduleV(T, T->top++, module);

    TValue o;
    setmoduleV(T, &o, module);
    copyTV(T, tea_tab_set(T, &T->modules, name, NULL), &o);
    
    T->top--;

    return module;
}

GCmodule* tea_submodule_new(tea_State* T, GCstr* name)
{
    GCmodule* module = tea_mem_newobj(T, GCmodule, TEA_TMODULE);
    tea_tab_init(&module->vars);
    tea_tab_init(&module->exports);
    module->name = name;
    module->path = NULL;
    return module;
}

GCrange* tea_range_new(tea_State* T, double start, double end, double step)
{
    GCrange* range = tea_mem_newobj(T, GCrange, TEA_TRANGE);
    range->start = start;
    range->end = end;
    range->step = step;
    return range;
}

GCclass* tea_class_new(tea_State* T, GCstr* name)
{
    GCclass* k = tea_mem_newobj(T, GCclass, TEA_TCLASS);
    k->name = name;
    k->super = T->object_class;
    setnilV(&k->init);
    tea_tab_init(&k->methods);
    return k;
}

GCinstance* tea_instance_new(tea_State* T, GCclass* klass)
{
    GCinstance* instance = tea_mem_newobj(T, GCinstance, TEA_TINSTANCE);
    instance->klass = klass;
    tea_tab_init(&instance->attrs);
    return instance;
}

GCmethod* tea_method_new(tea_State* T, TValue* receiver, GCfunc* func)
{
    GCmethod* bound = tea_mem_newobj(T, GCmethod, TEA_TMETHOD);
    copyTV(T, &bound->receiver, receiver);
    bound->func = func;
    return bound;
}

void TEA_FASTCALL tea_module_free(tea_State* T, GCmodule* module)
{
    tea_tab_free(T, &module->vars);
    tea_tab_free(T, &module->exports);
    tea_mem_freet(T, module);
}

void TEA_FASTCALL tea_range_free(tea_State* T, GCrange* range)
{
    tea_mem_freet(T, range);
}

void TEA_FASTCALL tea_class_free(tea_State* T, GCclass* klass)
{
    tea_tab_free(T, &klass->methods);
    tea_mem_freet(T, klass);
}

void TEA_FASTCALL tea_instance_free(tea_State* T, GCinstance* instance)
{
    tea_tab_free(T, &instance->attrs);
    tea_mem_freet(T, instance);
}

void TEA_FASTCALL tea_method_free(tea_State* T, GCmethod* method)
{
    tea_mem_freet(T, method);
}

/* Return pointer to object or its object data */
const void* tea_obj_pointer(cTValue* o)
{
    if(tvisudata(o))
        return ud_data(udataV(o));
    else if(tvispointer(o))
        return pointerV(o);
    else if(tvisgcv(o))
        return gcV(o);
    else
        return NULL;
}

static bool obj_range_equal(GCrange* a, GCrange* b)
{
    return a->start == b->start && a->end == b->end && a->step == b->step;
}

static bool obj_list_equal(GClist* a, GClist* b)
{
    if(a == b)
        return true;

    if(a->len != b->len)
        return false;

    for(int i = 0; i < a->len; i++)
    {
        if(!tea_obj_equal(list_slot(a, i), list_slot(b, i)))
        {
            return false;
        }
    }

    return true;
}

static bool obj_map_equal(GCmap* a, GCmap* b)
{
    if(a == b)
        return true;

    if(a->count != b->count)
        return false;

    if(a->count == 0)
        return true;

    for(int i = 0; i < a->size; i++)
    {
        MapEntry* entry = &a->entries[i];

        if(tvisnil(&entry->key))
        {
            continue;
        }

        cTValue* value = tea_map_get(b, &entry->key);
        if(!value)
        {
            return false;
        }

        if(!tea_obj_equal(&entry->val, value))
        {
            return false;
        }
    }

    return true;
}

/* Compare two values */
bool tea_obj_equal(cTValue* a, cTValue* b)
{
    if(itype(a) != itype(b))
        return false;
    switch(itype(a))
    {
        case TEA_TNIL:
            return true;
        case TEA_TBOOL:
            return boolV(a) == boolV(b);
        case TEA_TNUM:
            return numV(a) == numV(b);
        case TEA_TPOINTER:
            return pointerV(a) == pointerV(b);
        case TEA_TRANGE:
            return obj_range_equal(rangeV(a), rangeV(b));
        case TEA_TLIST:
            return obj_list_equal(listV(a), listV(b));
        case TEA_TMAP:
            return obj_map_equal(mapV(a), mapV(b));
        default:
            return gcV(a) == gcV(b);
    }
}

/* Compare two values without additional checks */
bool tea_obj_rawequal(cTValue* a, cTValue* b)
{
    if(itype(a) != itype(b))
        return false;
    switch(itype(a))
    {
        case TEA_TNIL:
            return true;
        case TEA_TBOOL:
            return boolV(a) == boolV(b);
        case TEA_TNUM:
            return numV(a) == numV(b);
        case TEA_TPOINTER:
            return pointerV(a) == pointerV(b);
        default:
            return gcV(a) == gcV(b);
    }
}

/* Attempt to convert a value into a number */
double tea_obj_tonum(TValue* o, bool* x)
{
    if(x != NULL)
        *x = true;
    switch(itype(o))
    {
        case TEA_TNIL:
            return 0;
        case TEA_TNUM:
            return numV(o);
        case TEA_TBOOL:
            return boolV(o) ? 1 : 0;
        case TEA_TSTR:
        {
            GCstr* str = strV(o);
            TValue tv;
            if(tea_strscan_num(str, &tv))
            {
                return tv.value.n;
            }
        /* Fallback */
        }
        default:
            if(x != NULL)
                *x = false;
            return 0;
    }
}