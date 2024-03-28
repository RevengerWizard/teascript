/*
** tea_obj.c
** Teascript values and objects
*/

#define tea_obj_c
#define TEA_CORE

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "tea_def.h"
#include "tea_gc.h"
#include "tea_obj.h"
#include "tea_map.h"
#include "tea_str.h"
#include "tea_tab.h"
#include "tea_state.h"
#include "tea_vm.h"
#include "tea_err.h"
#include "tea_strscan.h"

/* Value type names */
TEA_DATADEF const char* const tea_val_typenames[] = {
    "null", "bool", "number", "pointer",
    "string", "range", "function", "module", "class", "instance", "list", "map", "file"
};

TEA_DATADEF const char* const tea_obj_typenames[] = {
    "null", "bool", "number", "pointer", 
    "string", "range", "function",
    "module", "class", "instance", "list", "map", "file", 
    "proto", "upvalue", "method"
};

/* Allocate new GC object and link it to the objects root */
GCobj* tea_obj_alloc(tea_State* T, size_t size, uint8_t type)
{
    GCobj* obj = (GCobj*)tea_mem_realloc(T, NULL, 0, size);
    obj->gct = type;
    obj->marked = false;

    obj->next = T->gc.objects;
    T->gc.objects = obj;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p allocate %llu for %s\n", (void*)obj, size, tea_typename(setgcV(obj)));
#endif

    return obj;
}

GCmodule* tea_obj_new_module(tea_State* T, GCstr* name)
{
    char c = name->chars[0];

    TValue* module_val = tea_tab_get(&T->modules, name);
    if(c != '?' && module_val)
    {
        return moduleV(module_val);
    }

    GCmodule* module = tea_obj_new(T, GCmodule, TEA_TMODULE);
    tea_tab_init(&module->values);
    module->name = name;
    module->path = NULL;

    setmoduleV(T, T->top++, module);

    TValue v;
    setmoduleV(T, &v, module);
    TValue* o = tea_tab_set(T, &T->modules, name, NULL);
    copyTV(T, o, &v);
    
    T->top--;

    return module;
}

GCfile* tea_obj_new_file(tea_State* T, GCstr* path, GCstr* type)
{
    GCfile* file = tea_obj_new(T, GCfile, TEA_TFILE);
    file->path = path;
    file->type = type;
    file->is_open = true;
    return file;
}

GCrange* tea_obj_new_range(tea_State* T, double start, double end, double step)
{
    GCrange* range = tea_obj_new(T, GCrange, TEA_TRANGE);
    range->start = start;
    range->end = end;
    range->step = step;
    return range;
}

GCclass* tea_obj_new_class(tea_State* T, GCstr* name, GCclass* superclass)
{
    GCclass* k = tea_obj_new(T, GCclass, TEA_TCLASS);
    k->name = name;
    k->super = superclass;
    setnullV(&k->constructor);
    tea_tab_init(&k->statics);
    tea_tab_init(&k->methods);
    return k;
}

GCinstance* tea_obj_new_instance(tea_State* T, GCclass* klass)
{
    GCinstance* instance = tea_obj_new(T, GCinstance, TEA_TINSTANCE);
    instance->klass = klass;
    tea_tab_init(&instance->fields);
    return instance;
}

GCmethod* tea_obj_new_method(tea_State* T, TValue* receiver, GCfunc* method)
{
    GCmethod* bound = tea_obj_new(T, GCmethod, TEA_TMETHOD);
    copyTV(T, &bound->receiver, receiver);
    bound->method = method;
    return bound;
}

/* Return pointer to object or its object data */
const void* tea_obj_pointer(TValue* v)
{
    if(tvispointer(v))
        return pointerV(v);
    else if(tvisgcv(v))
        return gcV(v);
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

    if(a->count != b->count)
    {
        return false;
    }

    for(int i = 0; i < a->count; i++)
    {
        if(!tea_obj_equal(a->items + i, b->items + i))
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
    {
        return false;
    }

    if(a->count == 0)
    {
        return true;
    }

    for(int i = 0; i < a->size; i++)
    {
        MapEntry* entry = &a->entries[i];

        if(entry->empty)
        {
            continue;
        }

        TValue* value = tea_map_get(b, &entry->key);
        if(!value)
        {
            return false;
        }

        if(!tea_obj_equal(&entry->value, value))
        {
            return false;
        }
    }

    return true;
}

/* Compare two values */
bool tea_obj_equal(TValue* a, TValue* b)
{
    if(itype(a) != itype(b))
        return false;
    switch(itype(a))
    {
        case TEA_TNULL:
            return true;
        case TEA_TBOOL:
            return boolV(a) == boolV(b);
        case TEA_TNUMBER:
            return numberV(a) == numberV(b);
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
bool tea_obj_rawequal(TValue* a, TValue* b)
{
    if(itype(a) != itype(b))
        return false;
    switch(itype(a))
    {
        case TEA_TNULL:
            return true;
        case TEA_TBOOL:
            return boolV(a) == boolV(b);
        case TEA_TNUMBER:
            return numberV(a) == numberV(b);
        case TEA_TPOINTER:
            return pointerV(a) == pointerV(b);
        default:
            return gcV(a) == gcV(b);
    }
}

/* Attempt to convert a value into a number */
double tea_obj_tonumber(TValue* v, bool* x)
{
    if(x != NULL)
        *x = true;
    switch(itype(v))
    {
        case TEA_TNULL:
            return 0;
        case TEA_TNUMBER:
            return numberV(v);
        case TEA_TBOOL:
            return boolV(v) ? 1 : 0;
        case TEA_TSTRING:
        {
            GCstr* str = strV(v);
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