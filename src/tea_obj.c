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
    "null", "number", "bool",
    "string", "range", "function", "module", "class", "instance", "list", "map", "file"
};

TEA_DATADEF const char* const tea_obj_typenames[] = {
    "string", "range", "proto", "function", "module", "function", "upvalue",
    "class", "instance", "function", "list", "map", "file"
};

GCobj* tea_obj_alloc(tea_State* T, size_t size, ObjType type)
{
    GCobj* object = (GCobj*)tea_mem_realloc(T, NULL, 0, size);
    object->type = type;
    object->marked = false;

    object->next = T->objects;
    T->objects = object;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p allocate %llu for %s\n", (void*)object, size, tea_val_type(OBJECT_VAL(object)));
#endif

    return object;
}

GCmethod* tea_obj_new_method(tea_State* T, Value receiver, Value method)
{
    GCmethod* bound = tea_obj_new(T, GCmethod, OBJ_METHOD);
    bound->receiver = receiver;
    bound->method = method;

    return bound;
}

GCinstance* tea_obj_new_instance(tea_State* T, GCclass* klass)
{
    GCinstance* instance = tea_obj_new(T, GCinstance, OBJ_INSTANCE);
    instance->klass = klass;
    tea_tab_init(&instance->fields);

    return instance;
}

GCclass* tea_obj_new_class(tea_State* T, GCstr* name, GCclass* superclass)
{
    GCclass* klass = tea_obj_new(T, GCclass, OBJ_CLASS);
    klass->name = name;
    klass->super = superclass;
    klass->constructor = NULL_VAL;
    tea_tab_init(&klass->statics);
    tea_tab_init(&klass->methods);

    return klass;
}

GClist* tea_obj_new_list(tea_State* T)
{
    GClist* list = tea_obj_new(T, GClist, OBJ_LIST);
    list->items = NULL;
    list->count = 0;
    list->size = 0;

    return list;
}

void tea_list_append(tea_State* T, GClist* list, Value value)
{
    if(list->size < list->count + 1)
    {
        list->items = tea_mem_growvec(T, Value, list->items, list->size, INT_MAX);
    }
    list->items[list->count] = value;
    list->count++;
}

GCmodule* tea_obj_new_module(tea_State* T, GCstr* name)
{
    char c = name->chars[0];

    Value module_val;
    if(c != '?' && tea_tab_get(&T->modules, name, &module_val))
    {
        return AS_MODULE(module_val);
    }

    GCmodule* module = tea_obj_new(T, GCmodule, OBJ_MODULE);
    tea_tab_init(&module->values);
    module->name = name;
    module->path = NULL;

    tea_vm_push(T, OBJECT_VAL(module));
    tea_tab_set(T, &T->modules, name, OBJECT_VAL(module));
    tea_vm_pop(T, 1);

    return module;
}

GCfile* tea_obj_new_file(tea_State* T, GCstr* path, GCstr* type)
{
    GCfile* file = tea_obj_new(T, GCfile, OBJ_FILE);
    file->path = path;
    file->type = type;
    file->is_open = true;

    return file;
}

GCrange* tea_obj_new_range(tea_State* T, double start, double end, double step)
{
    GCrange* range = tea_obj_new(T, GCrange, OBJ_RANGE);
    range->start = start;
    range->end = end;
    range->step = step;

    return range;
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

    for(int i = 0; i < list->count; i++)
    {
        Value value = list->items[i];

        char* element;
        int element_size;

        if(tea_val_rawequal(value, OBJECT_VAL(list)))
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

        if(tea_val_rawequal(entry->key, OBJECT_VAL(map)))
        {
            key = "{...}";
            key_size = 5;
        }
        else
        {
            GCstr* s = tea_val_tostring(T, entry->key, depth);
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

        if(!IS_STRING(entry->key))
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

        if(tea_val_rawequal(entry->value, OBJECT_VAL(map)))
        {
            element = "{...}";
            element_size = 5;
        }
        else
        {
            GCstr* s = tea_val_tostring(T, entry->value, depth);
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
    Value tostring;
    GCstr* _tostring = T->opm_name[MM_TOSTRING];
    if(tea_tab_get(&instance->klass->methods, _tostring, &tostring))
    {
        tea_vm_push(T, OBJECT_VAL(instance));
        tea_vm_call(T, tostring, 0);

        Value result = tea_vm_pop(T, 1);
        if(!IS_STRING(result))
        {
            tea_err_run(T, "'tostring' must return a string");
        }

        return AS_STRING(result);
    }

    return tea_str_format(T, "<@ instance>", instance->klass->name);
}

static GCstr* obj_tostring(tea_State* T, Value value, int depth)
{
    if(depth > MAX_TOSTRING_DEPTH)
    {
        return tea_str_lit(T, "...");
    }

    depth++;

    switch(OBJECT_TYPE(value))
    {
        case OBJ_FILE:
            return tea_str_lit(T, "<file>");
        case OBJ_METHOD:
            return tea_str_lit(T, "<method>");
        case OBJ_CFUNC:
        {
            switch(AS_CFUNC(value)->type)
            {
                case C_PROPERTY:
                    return tea_str_lit(T, "<property>");
                case C_FUNCTION:
                case C_METHOD:
                    return tea_str_lit(T, "<function>");
            }
        }
        case OBJ_PROTO:
            return obj_function_tostring(T, AS_PROTO(value));
        case OBJ_FUNC:
            return obj_function_tostring(T, AS_FUNC(value)->proto);
        case OBJ_LIST:
            return obj_list_tostring(T, AS_LIST(value), depth);
        case OBJ_MAP:
            return obj_map_tostring(T, AS_MAP(value), depth);
        case OBJ_RANGE:
            return obj_range_tostring(T, AS_RANGE(value));
        case OBJ_MODULE:
            return tea_str_format(T, "<@ module>", AS_MODULE(value)->name);
        case OBJ_CLASS:
            return tea_str_format(T, "<@>", AS_CLASS(value)->name);
        case OBJ_INSTANCE:
            return obj_instance_tostring(T, AS_INSTANCE(value));
        case OBJ_STRING:
            return AS_STRING(value);
        case OBJ_UPVALUE:
            return tea_str_lit(T, "<upvalue>");
        default:
            break;
    }
    return tea_str_lit(T, "unknown");
}

GCstr* tea_val_tostring(tea_State* T, Value value, int depth)
{
#ifdef TEA_NAN_TAGGING
    if(IS_BOOL(value))
    {
        return AS_BOOL(value) ? tea_str_lit(T, "true") : tea_str_lit(T, "false");
    }
    else if(IS_NULL(value))
    {
        return tea_str_lit(T, "null");
    }
    else if(IS_NUMBER(value))
    {
        return val_numtostring(T, AS_NUMBER(value));
    }
    else if(IS_OBJECT(value))
    {
        return obj_tostring(T, value, depth);
    }
#else
    switch(value.type)
    {
        case VAL_BOOL:
            return AS_BOOL(value) ? tea_str_lit(T, "true") : tea_str_lit(T, "false");
        case VAL_NULL:
            return tea_str_lit(T, "null");
        case VAL_NUMBER:
            return val_numtostring(T, AS_NUMBER(value));
        case VAL_OBJECT:
            return obj_tostring(T, value);
        default:
            break;
    }
#endif
    return tea_str_lit(T, "unknown");
}

static bool obj_range_equals(GCrange* a, GCrange* b)
{
    return a->start == b->start && a->end == b->end && a->step == b->step;
}

static bool obj_list_equals(GClist* a, GClist* b)
{
    if(a->count != b->count)
    {
        return false;
    }

    for(int i = 0; i < a->count; ++i)
    {
        if(!tea_val_equal(a->items[i], b->items[i]))
        {
            return false;
        }
    }

    return true;
}

static bool obj_map_equals(GCmap* a, GCmap* b)
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

        Value value;
        if(!tea_map_get(b, entry->key, &value))
        {
            return false;
        }

        if(!tea_val_equal(entry->value, value))
        {
            return false;
        }
    }

    return true;
}

static bool obj_equal(Value a, Value b)
{
    if(OBJECT_TYPE(a) != OBJECT_TYPE(b)) return false;

    switch(OBJECT_TYPE(a))
    {
        case OBJ_RANGE:
            return obj_range_equals(AS_RANGE(a), AS_RANGE(b));
        case OBJ_LIST:
            return obj_list_equals(AS_LIST(a), AS_LIST(b));
        case OBJ_MAP:
            return obj_map_equals(AS_MAP(a), AS_MAP(b));
        default:
            break;
    }
    return AS_OBJECT(a) == AS_OBJECT(b);
}

bool tea_val_equal(Value a, Value b)
{
#ifdef TEA_NAN_TAGGING
    if(IS_NUMBER(a) && IS_NUMBER(b))
    {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    else if(IS_OBJECT(a) && IS_OBJECT(b))
    {
        return obj_equal(a, b);
    }
    return a == b;
#else
    if(a.type != b.type)
        return false;
    switch(a.type)
    {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJECT:
            return obj_equal(a, b);
        default:
            return false; /* Unreachable */
    }
#endif
}

const char* tea_val_type(Value a)
{
#ifdef TEA_NAN_TAGGING
    if(IS_BOOL(a))
    {
        return "bool";
    }
    else if(IS_NULL(a))
    {
        return "null";
    }
    else if(IS_NUMBER(a))
    {
        return "number";
    }
    else if(IS_OBJECT(a))
    {
        return tea_obj_typenames[OBJECT_TYPE(a)];
    }
#else
    switch(a.type)
    {
        case VAL_BOOL:
            return "bool";
        case VAL_NULL:
            return "null";
        case VAL_NUMBER:
            return "number";
        case VAL_OBJECT:
            return tea_obj_typenames[OBJECT_TYPE(a)];
        default:
            break;
    }
#endif
    return "unknown";
}

bool tea_val_rawequal(Value a, Value b)
{
#ifdef TEA_NAN_TAGGING
    if(IS_NUMBER(a) && IS_NUMBER(b))
    {
        return AS_NUMBER(a) == AS_NUMBER(b);
    }
    return a == b;
#else
    if(a.type != b.type)
        return false;
    switch(a.type)
    {
        case VAL_BOOL:
            return AS_BOOL(a) == AS_BOOL(b);
        case VAL_NULL:
            return true;
        case VAL_NUMBER:
            return AS_NUMBER(a) == AS_NUMBER(b);
        case VAL_OBJECT:
            return AS_OBJECT(a) == AS_OBJECT(b);
        default:
            return false; /* Unreachable */
    }
#endif
}

double tea_val_tonumber(Value value, bool* x)
{
    if(x != NULL)
        *x = true;
    if(IS_NUMBER(value))
    {
        return AS_NUMBER(value);
    }
    else if(IS_BOOL(value))
    {
        return AS_BOOL(value) ? 1 : 0;
    }
    else if(IS_STRING(value))
    {
        char* n = AS_CSTRING(value);
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
    }
    else
    {
        if(x != NULL)
            *x = false;
        return 0;
    }
}