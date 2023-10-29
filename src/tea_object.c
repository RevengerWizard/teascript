/*
** tea_object.c
** Teascript object model and functions
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define tea_object_c
#define TEA_CORE

#include "tea_memory.h"
#include "tea_object.h"
#include "tea_map.h"
#include "tea_string.h"
#include "tea_table.h"
#include "tea_value.h"
#include "tea_state.h"
#include "tea_vm.h"
#include "tea_do.h"

TeaObject* tea_obj_allocate(TeaState* T, size_t size, TeaObjectType type)
{
    TeaObject* object = (TeaObject*)tea_mem_realloc(T, NULL, 0, size);
    object->type = type;
    object->is_marked = false;

    object->next = T->objects;
    T->objects = object;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p allocate %zu for %s\n", (void*)object, size, tea_val_type(OBJECT_VAL(object)));
#endif

    return object;
}

TeaOBoundMethod* tea_obj_new_bound_method(TeaState* T, TeaValue receiver, TeaValue method)
{
    TeaOBoundMethod* bound = TEA_ALLOCATE_OBJECT(T, TeaOBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;

    return bound;
}

TeaOInstance* tea_obj_new_instance(TeaState* T, TeaOClass* klass)
{
    TeaOInstance* instance = TEA_ALLOCATE_OBJECT(T, TeaOInstance, OBJ_INSTANCE);
    instance->klass = klass;
    tea_tab_init(&instance->fields);

    return instance;
}

TeaOClass* tea_obj_new_class(TeaState* T, TeaOString* name, TeaOClass* superclass)
{
    TeaOClass* klass = TEA_ALLOCATE_OBJECT(T, TeaOClass, OBJ_CLASS);
    klass->name = name;
    klass->super = superclass;
    klass->constructor = NULL_VAL;
    tea_tab_init(&klass->statics);
    tea_tab_init(&klass->methods);

    return klass;
}

TeaOUserdata* tea_obj_new_userdata(TeaState* T, size_t size)
{
    TeaOUserdata* ud = TEA_ALLOCATE_OBJECT(T, TeaOUserdata, OBJ_USERDATA);

    if(size > 0)
    {
        ud->data = tea_mem_realloc(T, NULL, 0, size);
    }
    else
    {
        ud->data = NULL;
    }

    ud->size = size;

    return ud;
}

TeaOList* tea_obj_new_list(TeaState* T)
{
    TeaOList* list = TEA_ALLOCATE_OBJECT(T, TeaOList, OBJ_LIST);
    tea_init_value_array(&list->items);

    return list;
}

TeaOModule* tea_obj_new_module(TeaState* T, TeaOString* name)
{
    char c = name->chars[0];

    TeaValue module_val;
    if(c != '?')
    {
        if(tea_tab_get(&T->modules, name, &module_val))
        {
            return AS_MODULE(module_val);
        }
    }

    TeaOModule* module = TEA_ALLOCATE_OBJECT(T, TeaOModule, OBJ_MODULE);
    tea_tab_init(&module->values);
    module->name = name;
    module->path = NULL;

    tea_vm_push(T, OBJECT_VAL(module));
    tea_tab_set(T, &T->modules, name, OBJECT_VAL(module));
    tea_vm_pop(T, 1);

    return module;
}

TeaOFile* tea_obj_new_file(TeaState* T, TeaOString* path, TeaOString* type)
{
    TeaOFile* file = TEA_ALLOCATE_OBJECT(T, TeaOFile, OBJ_FILE);
    file->path = path;
    file->type = type;
    file->is_open = true;

    return file;
}

TeaORange* tea_obj_new_range(TeaState* T, double start, double end, double step)
{
    TeaORange* range = TEA_ALLOCATE_OBJECT(T, TeaORange, OBJ_RANGE);
    range->start = start;
    range->end = end;
    range->step = step;

    return range;
}

static TeaOString* function_tostring(TeaState* T, TeaOFunction* function)
{
    if(function->name == NULL)
        return tea_str_literal(T, "<script>");

    return tea_str_literal(T, "<function>");
}

static TeaOString* list_tostring(TeaState* T, TeaOList* list, int depth)
{
    if(list->items.count == 0)
        return tea_str_literal(T, "[]");

    int size = 50;

    char* string = TEA_ALLOCATE(T, char, size);
    memcpy(string, "[", 1);
    int length = 1;

    for(int i = 0; i < list->items.count; i++)
    {
        TeaValue value = list->items.values[i];

        char* element;
        int element_size;

        if(tea_val_rawequal(value, OBJECT_VAL(list)))
        {
            element = "[...]";
            element_size = 5;
        }
        else
        {
            TeaOString* s = tea_val_tostring(T, value, depth);
            element = s->chars;
            element_size = s->length;
        }

        if(element_size > (size - length - 6))
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

            string = TEA_GROW_ARRAY(T, char, string, old_size, size);
        }

        memcpy(string + length, element, element_size);
        length += element_size;

        if(i != list->items.count - 1)
        {
            memcpy(string + length, ", ", 2);
            length += 2;
        }
    }

    memcpy(string + length, "]", 1);
    length += 1;
    string[length] = '\0';

    string = TEA_GROW_ARRAY(T, char, string, size, length + 1);

    return tea_str_take(T, string, length);
}

#define MAX_TOSTRING_DEPTH 16

static TeaOString* map_tostring(TeaState* T, TeaOMap* map, int depth)
{
    if(map->count == 0)
        return tea_str_literal(T, "{}");
    
    if(depth > MAX_TOSTRING_DEPTH)
        return tea_str_literal(T, "{...}");

    int count = 0;
    int size = 50;

    char* string = TEA_ALLOCATE(T, char, size);
    memcpy(string, "{", 1);
    int length = 1;

    for(int i = 0; i < map->capacity; i++)
    {
        TeaMapItem* item = &map->items[i];
        if(item->empty)
        {
            continue;
        }

        count++;

        char* key;
        int key_size;

        if(tea_val_rawequal(item->key, OBJECT_VAL(map)))
        {
            key = "{...}";
            key_size = 5;
        }
        else
        {
            TeaOString* s = tea_val_tostring(T, item->key, depth);
            key = s->chars;
            key_size = s->length;
        }

        if(key_size > (size - length - key_size - 4))
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

            string = TEA_GROW_ARRAY(T, char, string, old_size, size);
        }

        if(!IS_STRING(item->key))
        {
            memcpy(string + length, "[", 1);
            memcpy(string + length + 1, key, key_size);
            memcpy(string + length + 1 + key_size, "] = ", 4);
            length += 5 + key_size;
        }
        else
        {
            memcpy(string + length, key, key_size);
            memcpy(string + length + key_size, " = ", 3);
            length += 3 + key_size;
        }

        char* element;
        int element_size;

        if(tea_val_rawequal(item->value, OBJECT_VAL(map)))
        {
            element = "{...}";
            element_size = 5;
        }
        else
        {
            TeaOString* s = tea_val_tostring(T, item->value, depth);
            element = s->chars;
            element_size = s->length;
        }

        if(element_size > (size - length - element_size - 6))
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

            string = TEA_GROW_ARRAY(T, char, string, old_size, size);
        }

        memcpy(string + length, element, element_size);
        length += element_size;

        if(count != map->count)
        {
            memcpy(string + length, ", ", 2);
            length += 2;
        }
    }

    memcpy(string + length, "}", 1);
    length += 1;
    string[length] = '\0';

    string = TEA_GROW_ARRAY(T, char, string, size, length + 1);

    return tea_str_take(T, string, length);
}

static TeaOString* range_tostring(TeaState* T, TeaORange* range)
{
    TeaOString* start = tea_val_number_tostring(T, range->start);
    TeaOString* end = tea_val_number_tostring(T, range->end);

    return tea_str_format(T, "@...@", start, end);
}

static TeaOString* instance_tostring(TeaState* T, TeaOInstance* instance)
{
    TeaValue tostring;
    TeaOString* _tostring = T->opm_name[MT_TOSTRING];
    if(tea_tab_get(&instance->klass->methods, _tostring, &tostring))
    {
        tea_vm_push(T, OBJECT_VAL(instance));
        tea_do_call(T, tostring, 0);

        TeaValue result = tea_vm_pop(T, 1);
        if(!IS_STRING(result))
        {
            tea_vm_error(T, "'tostring' must return a string");
        }

        return AS_STRING(result);
    }

    return tea_str_format(T, "<@ instance>", instance->klass->name);
}

TeaOString* tea_obj_tostring(TeaState* T, TeaValue value, int depth)
{
    if(depth > MAX_TOSTRING_DEPTH)
    {
        return tea_str_literal(T, "...");
    }

    depth++;

    switch(OBJECT_TYPE(value))
    {
        case OBJ_FILE:
            return tea_str_literal(T, "<file>");
        case OBJ_BOUND_METHOD:
            return tea_str_literal(T, "<method>");
        case OBJ_NATIVE:
        {
            switch(AS_NATIVE(value)->type)
            {
                case NATIVE_PROPERTY:
                    return tea_str_literal(T, "<property>");
                case NATIVE_FUNCTION:
                case NATIVE_METHOD:
                    return tea_str_literal(T, "<function>");
            }
        }
        case OBJ_FUNCTION:
            return function_tostring(T, AS_FUNCTION(value));
        case OBJ_CLOSURE:
            return function_tostring(T, AS_CLOSURE(value)->function);
        case OBJ_LIST:
            return list_tostring(T, AS_LIST(value), depth);
        case OBJ_MAP:
            return map_tostring(T, AS_MAP(value), depth);
        case OBJ_RANGE:
            return range_tostring(T, AS_RANGE(value));
        case OBJ_MODULE:
            return tea_str_format(T, "<@ module>", AS_MODULE(value)->name);
        case OBJ_CLASS:
            return tea_str_format(T, "<@>", AS_CLASS(value)->name);
        case OBJ_INSTANCE:
            return instance_tostring(T, AS_INSTANCE(value));
        case OBJ_STRING:
            return AS_STRING(value);
        case OBJ_UPVALUE:
            return tea_str_literal(T, "<upvalue>");
        default:
            break;
    }
    return tea_str_literal(T, "unknown");
}

static bool range_equals(TeaORange* a, TeaORange* b)
{
    return a->start == b->start && a->end == b->end && a->step == b->step;
}

static bool list_equals(TeaOList* a, TeaOList* b)
{
    if(a->items.count != b->items.count)
    {
        return false;
    }

    for(int i = 0; i < a->items.count; ++i)
    {
        if(!tea_val_equal(a->items.values[i], b->items.values[i]))
        {
            return false;
        }
    }

    return true;
}

static bool map_equals(TeaOMap* a, TeaOMap* b)
{
    if(a->count != b->count)
    {
        return false;
    }

    if(a->count == 0)
    {
        return true;
    }

    for(int i = 0; i < a->capacity; i++)
    {
        TeaMapItem* item = &a->items[i];

        if(item->empty)
        {
            continue;
        }

        TeaValue value;
        if(!tea_map_get(b, item->key, &value))
        {
            return false;
        }

        if(!tea_val_equal(item->value, value))
        {
            return false;
        }
    }

    return true;
}

bool tea_obj_equal(TeaValue a, TeaValue b)
{
    if(OBJECT_TYPE(a) != OBJECT_TYPE(b)) return false;

    switch(OBJECT_TYPE(a))
    {
        case OBJ_RANGE:
            return range_equals(AS_RANGE(a), AS_RANGE(b));
        case OBJ_LIST:
            return list_equals(AS_LIST(a), AS_LIST(b));
        case OBJ_MAP:
            return map_equals(AS_MAP(a), AS_MAP(b));
        default:
            break;
    }
    return AS_OBJECT(a) == AS_OBJECT(b);
}

const char* tea_obj_type(TeaValue a)
{
    switch(OBJECT_TYPE(a))
    {
        case OBJ_USERDATA:
            return "userdata";
        case OBJ_UPVALUE:
            return "upvalue";
        case OBJ_FILE:
            return "file";
        case OBJ_RANGE:
            return "range";
        case OBJ_MODULE:
            return "module";
        case OBJ_CLASS:
            return "class";
        case OBJ_BOUND_METHOD:
            return "method";
        case OBJ_INSTANCE:
            return "instance";
        case OBJ_STRING:
            return "string";
        case OBJ_LIST:
            return "list";
        case OBJ_MAP:
            return "map";
        case OBJ_NATIVE:
        {
            switch(AS_NATIVE(a)->type)
            {
                case NATIVE_FUNCTION:
                case NATIVE_METHOD:
                    return "function";
                case NATIVE_PROPERTY:
                    return "property";
            }
        }
        case OBJ_CLOSURE:
        case OBJ_FUNCTION:
            return "function";
        default:
            break;
    }
    return "unknown";
}