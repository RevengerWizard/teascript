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
        T->opm_name[i] = s;
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

GCclass* tea_meta_getclass(tea_State* T, cTValue* o)
{
    switch(itype(o))
    {
        case TEA_TNUM:
            return T->number_class;
        case TEA_TBOOL:
            return T->bool_class;
        case TEA_TFUNC:
            return T->func_class;
        case TEA_TUDATA:
        case TEA_TINSTANCE:
            return instanceV(o)->klass;
        case TEA_TLIST: 
            return T->list_class;
        case TEA_TMAP: 
            return T->map_class;
        case TEA_TSTR: 
            return T->string_class;
        case TEA_TRANGE: 
            return T->range_class;
        default: 
            return NULL;
    }
    return NULL;
}

bool tea_meta_isclass(tea_State* T, GCclass* klass)
{
    return (klass == T->number_class ||
            klass == T->bool_class ||
            klass == T->func_class ||
            klass == T->list_class ||
            klass == T->map_class ||
            klass == T->string_class ||
            klass == T->range_class);
}