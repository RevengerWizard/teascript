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

/* Value type names */
TEA_DATADEF const char* const tea_val_typenames[] = {
    "null", "bool", "number", "pointer",
    "string", "range", "function", "module", "class", "instance", "list", "map", "file"
};

TEA_DATADEF const char* const tea_obj_typenames[] = {
    "null", "bool", "number", "pointer", 
    "string", "range", "function",
    "function", "module", "class", "instance", "method", "list", "map", "file", "proto", "upvalue"
};

/* Allocate new GC object and link it to the objects root */
GCobj* tea_obj_alloc(tea_State* T, size_t size, uint8_t type)
{
    GCobj* obj = (GCobj*)tea_mem_realloc(T, NULL, 0, size);
    obj->gct = type;
    obj->marked = false;

    obj->next = T->objects;
    T->objects = obj;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p allocate %llu for %s\n", (void*)obj, size, tea_val_type(setgcV(obj)));
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

GCmethod* tea_obj_new_method(tea_State* T, TValue* receiver, TValue* method)
{
    GCmethod* bound = tea_obj_new(T, GCmethod, TEA_TMETHOD);
    copyTV(T, &bound->receiver, receiver);
    copyTV(T, &bound->method, method);
    return bound;
}

static GCstr* obj_function_tostring(tea_State* T, GCproto* proto)
{
    if(proto->type > PROTO_FUNCTION)
    {
        return proto->name;
    }

    return tea_str_lit(T, "<function>");
}

static GCstr* obj_list_tostring(tea_State* T, GClist* list, int depth)
{
    if(list->count == 0)
        return tea_str_lit(T, "[]");

    int size = 50;

    char* string = tea_mem_new(T, char, size);
    memcpy(string, "[", 1);
    int len = 1;

    TValue v;
    for(int i = 0; i < list->count; i++)
    {
        TValue* value = list->items + i;

        char* element;
        int element_size;

        setlistV(T, &v, list);
        if(tea_val_rawequal(value, &v))
        {
            element = "[...]";
            element_size = 5;
        }
        else
        {
            GCstr* s = tea_val_tostring(T, value, depth);
            element = s->chars;
            element_size = s->len;
        }

        if(element_size > (size - len - 6))
        {
            int old_size = size;
            if(element_size > size)
            {
                size = size + element_size * 2 + 6;
            }
            else
            {
                size = size * 2 + 6;
            }

            string = tea_mem_reallocvec(T, char, string, old_size, size);
        }

        memcpy(string + len, element, element_size);
        len += element_size;

        if(i != list->count - 1)
        {
            memcpy(string + len, ", ", 2);
            len += 2;
        }
    }

    memcpy(string + len, "]", 1);
    len += 1;
    string[len] = '\0';

    string = tea_mem_reallocvec(T, char, string, size, len + 1);

    return tea_str_take(T, string, len);
}

#define MAX_TOSTRING_DEPTH 16

static GCstr* obj_map_tostring(tea_State* T, GCmap* map, int depth)
{
    if(map->count == 0)
        return tea_str_lit(T, "{}");
    
    if(depth > MAX_TOSTRING_DEPTH)
        return tea_str_lit(T, "{...}");

    int count = 0;
    int size = 50;

    char* string = tea_mem_new(T, char, size);
    memcpy(string, "{", 1);
    int len = 1;

    TValue v;
    for(int i = 0; i < map->size; i++)
    {
        MapEntry* entry = &map->entries[i];
        if(entry->empty)
        {
            continue;
        }

        count++;

        char* key;
        int key_size;

        setmapV(T, &v, map);
        if(tea_val_rawequal(&entry->key, &v))
        {
            key = "{...}";
            key_size = 5;
        }
        else
        {
            GCstr* s = tea_val_tostring(T, &entry->key, depth);
            key = s->chars;
            key_size = s->len;
        }

        if(key_size > (size - len - key_size - 4))
        {
            int old_size = size;
            if(key_size > size)
            {
                size += key_size * 2 + 4;
            }
            else
            {
                size *= 2 + 4;
            }

            string = tea_mem_reallocvec(T, char, string, old_size, size);
        }

        if(!tvisstr(&entry->key))
        {
            memcpy(string + len, "[", 1);
            memcpy(string + len + 1, key, key_size);
            memcpy(string + len + 1 + key_size, "] = ", 4);
            len += 5 + key_size;
        }
        else
        {
            memcpy(string + len, key, key_size);
            memcpy(string + len + key_size, " = ", 3);
            len += 3 + key_size;
        }

        char* element;
        int element_size;

        setmapV(T, &v, map);
        if(tea_val_rawequal(&entry->value, &v))
        {
            element = "{...}";
            element_size = 5;
        }
        else
        {
            GCstr* s = tea_val_tostring(T, &entry->value, depth);
            element = s->chars;
            element_size = s->len;
        }

        if(element_size > (size - len - element_size - 6))
        {
            int old_size = size;
            if(element_size > size)
            {
                size += element_size * 2 + 6;
            }
            else
            {
                size = size * 2 + 6;
            }

            string = tea_mem_reallocvec(T, char, string, old_size, size);
        }

        memcpy(string + len, element, element_size);
        len += element_size;

        if(count != map->count)
        {
            memcpy(string + len, ", ", 2);
            len += 2;
        }
    }

    memcpy(string + len, "}", 1);
    len += 1;
    string[len] = '\0';

    string = tea_mem_reallocvec(T, char, string, size, len + 1);

    return tea_str_take(T, string, len);
}

static GCstr* val_numtostring(tea_State* T, double number)
{
    if(isnan(number)) return tea_str_lit(T, "nan");
    if(isinf(number))
    {
        if(number > 0.0)
        {
            return tea_str_lit(T, "infinity");
        }
        else
        {
            return tea_str_lit(T, "-infinity");
        }
    }

    int len = snprintf(NULL, 0, TEA_NUMBER_FMT, number);
    char* string = tea_mem_new(T, char, len + 1);
    snprintf(string, len + 1, TEA_NUMBER_FMT, number);

    return tea_str_take(T, string, len);
}

static GCstr* obj_range_tostring(tea_State* T, GCrange* range)
{
    GCstr* start = val_numtostring(T, range->start);
    GCstr* end = val_numtostring(T, range->end);

    return tea_str_format(T, "@..@", start, end);
}

static GCstr* obj_instance_tostring(tea_State* T, GCinstance* instance)
{
    GCstr* _tostring = T->opm_name[MM_TOSTRING];
    TValue* tostring = tea_tab_get(&instance->klass->methods, _tostring);
    if(tostring)
    {
        setinstanceV(T, T->top++, instance);
        tea_vm_call(T, tostring, 0);

        TValue* result = T->top--;
        if(!tvisstr(result))
        {
            tea_err_run(T, TEA_ERR_TOSTR);
        }

        return strV(result);
    }

    return tea_str_format(T, "<@ instance>", instance->klass->name);
}

GCstr* tea_val_tostring(tea_State* T, TValue* value, int depth)
{
    if(depth > MAX_TOSTRING_DEPTH)
    {
        return tea_str_lit(T, "...");
    }

    depth++;

    switch(itype(value))
    {
        case TEA_TNULL:
            return tea_str_lit(T, "null");
        case TEA_TBOOL:
            return boolV(value) ? tea_str_lit(T, "true") : tea_str_lit(T, "false");
        case TEA_TNUMBER:
            return val_numtostring(T, numberV(value));
        case TEA_TPOINTER:
            return tea_str_lit(T, "pointer");
        case TEA_TFILE:
            return tea_str_lit(T, "<file>");
        case TEA_TMETHOD:
            return tea_str_lit(T, "<method>");
        case TEA_TCFUNC:
            return tea_str_lit(T, "<function>");
        case TEA_TPROTO:
            return obj_function_tostring(T, protoV(value));
        case TEA_TFUNC:
            return obj_function_tostring(T, funcV(value)->proto);
        case TEA_TLIST:
            return obj_list_tostring(T, listV(value), depth);
        case TEA_TMAP:
            return obj_map_tostring(T, mapV(value), depth);
        case TEA_TRANGE:
            return obj_range_tostring(T, rangeV(value));
        case TEA_TMODULE:
            return tea_str_format(T, "<@ module>", moduleV(value)->name);
        case TEA_TCLASS:
            return tea_str_format(T, "<@>", classV(value)->name);
        case TEA_TINSTANCE:
            return obj_instance_tostring(T, instanceV(value));
        case TEA_TSTRING:
            return strV(value);
        case TEA_TUPVALUE:
            return tea_str_lit(T, "<upvalue>");
        default:
            break;
    }
    return tea_str_lit(T, "unknown");
}

static bool obj_range_equal(GCrange* a, GCrange* b)
{
    return a->start == b->start && a->end == b->end && a->step == b->step;
}

static bool obj_list_equal(GClist* a, GClist* b)
{
    if(a->count != b->count)
    {
        return false;
    }

    for(int i = 0; i < a->count; i++)
    {
        if(!tea_val_equal(a->items + i, b->items + i))
        {
            return false;
        }
    }

    return true;
}

static bool obj_map_equal(GCmap* a, GCmap* b)
{
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

        if(!tea_val_equal(&entry->value, value))
        {
            return false;
        }
    }

    return true;
}

bool tea_val_equal(TValue* a, TValue* b)
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

const char* tea_val_type(TValue* a)
{
    return tea_obj_typenames[itype(a)];
}

bool tea_val_rawequal(TValue* a, TValue* b)
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

double tea_val_tonumber(TValue* v, bool* x)
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
            char* n = strV(v)->chars;
            char* end;
            errno = 0;

            double number = strtod(n, &end);

            if(errno != 0 || *end != '\0')
            {
                if(x != NULL)
                    *x = false;
                return 0;
            }
            return number;
        default:
            if(x != NULL)
                *x = false;
            return 0;
    }
}