// tea_object.c
// Teascript object model and functions

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tea_memory.h"
#include "tea_object.h"
#include "tea_table.h"
#include "tea_value.h"
#include "tea_state.h"

TeaObject* tea_allocate_object(TeaState* T, size_t size, TeaObjectType type)
{
    TeaObject* object = (TeaObject*)tea_reallocate(T, NULL, 0, size);
    object->type = type;
    object->is_marked = false;

    object->next = T->objects;
    T->objects = object;

#ifdef TEA_DEBUG_LOG_GC
    printf("%p allocate %zu for %s\n", (void*)object, size, tea_value_type(OBJECT_VAL(object)));
#endif

    return object;
}

TeaObjectNative* tea_new_native(TeaState* T, TeaNativeType type, TeaCFunction fn)
{
    TeaObjectNative* native = ALLOCATE_OBJECT(T, TeaObjectNative, OBJ_NATIVE);
    native->type = type;
    native->fn = fn;

    return native;
}

TeaObjectThread* tea_new_thread(TeaState* T, TeaObjectClosure* closure)
{
    int stack_capacity = closure == NULL ? TEA_MIN_SLOTS : tea_closest_power_of_two(closure->function->max_slots + 1);
    TeaValue* stack = ALLOCATE(T, TeaValue, stack_capacity);

    TeaCallFrame* frames = ALLOCATE(T, TeaCallFrame, 64);
    TeaObjectThread* thread = ALLOCATE_OBJECT(T, TeaObjectThread, OBJ_THREAD);

    thread->stack = stack;
    thread->stack_top = thread->stack;
    thread->stack_capacity = stack_capacity;

    thread->frames = frames;
    thread->frame_capacity = 64;
    thread->frame_count = 0;

    thread->parent = NULL;
    thread->open_upvalues = NULL;

    if(closure != NULL)
    {
        tea_append_callframe(T, thread, closure, thread->stack);
        
        thread->stack_top[0] = OBJECT_VAL(closure);
        thread->stack_top++;
    }

    return thread;
}

void tea_ensure_stack(TeaState* T, TeaObjectThread* thread, int needed)
{
    if(thread->stack_capacity >= needed) return;

	int capacity = (int)tea_closest_power_of_two((int)needed);
	TeaValue* old_stack = thread->stack;

	thread->stack = (TeaValue*)tea_reallocate(T, thread->stack, sizeof(TeaValue) * thread->stack_capacity, sizeof(TeaValue) * capacity);
	thread->stack_capacity = capacity;

	if(thread->stack != old_stack)
    {
		for(int i = 0; i < thread->frame_capacity; i++)
        {
			TeaCallFrame* frame = &thread->frames[i];
			frame->slots = thread->stack + (frame->slots - old_stack);
		}

		for(TeaObjectUpvalue* upvalue = thread->open_upvalues; upvalue != NULL; upvalue = upvalue->next)
        {
			upvalue->location = thread->stack + (upvalue->location - old_stack);
		}

		thread->stack_top = thread->stack + (thread->stack_top - old_stack);
	}
}

TeaObjectUserdata* tea_new_userdata(TeaState* T, size_t size)
{
    TeaObjectUserdata* userdata = ALLOCATE_OBJECT(T, TeaObjectUserdata, OBJ_USERDATA);
    if(size > 0)
    {
        userdata->data = tea_reallocate(T, NULL, 0, size);
    }
    else
    {
        userdata->data = NULL;
    }

    userdata->size = size;
    userdata->fn = NULL;

    return userdata;
}

TeaObjectRange* tea_new_range(TeaState* T, double start, double end, double step)
{
    TeaObjectRange* range = ALLOCATE_OBJECT(T, TeaObjectRange, OBJ_RANGE);
    range->start = start;
    range->end = end;
    range->step = step;

    return range;
}

TeaObjectFile* tea_new_file(TeaState* T, TeaObjectString* path, TeaObjectString* type)
{
    TeaObjectFile* file = ALLOCATE_OBJECT(T, TeaObjectFile, OBJ_FILE);
    file->path = path;
    file->type = type;
    file->is_open = true;

    return file;
}

TeaObjectModule* tea_new_module(TeaState* T, TeaObjectString* name)
{
    TeaValue module_val;
    if(tea_table_get(&T->modules, name, &module_val)) 
    {
        return AS_MODULE(module_val);
    }

    TeaObjectModule* module = ALLOCATE_OBJECT(T, TeaObjectModule, OBJ_MODULE);
    tea_init_table(&module->values);
    module->name = name;
    module->path = NULL;
    
    tea_push_slot(T, OBJECT_VAL(module));
    tea_table_set(T, &T->modules, name, OBJECT_VAL(module));
    tea_pop_slot(T);

    return module;
}

TeaObjectList* tea_new_list(TeaState* T)
{
    TeaObjectList* list = ALLOCATE_OBJECT(T, TeaObjectList, OBJ_LIST);
    tea_init_value_array(&list->items);

    return list;
}

TeaObjectMap* tea_new_map(TeaState* T)
{
    TeaObjectMap* map = ALLOCATE_OBJECT(T, TeaObjectMap, OBJ_MAP);
    map->count = 0;
    map->capacity = 0;
    map->items = NULL;
    
    return map;
}

TeaObjectBoundMethod* tea_new_bound_method(TeaState* T, TeaValue receiver, TeaValue method)
{
    TeaObjectBoundMethod* bound = ALLOCATE_OBJECT(T, TeaObjectBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;

    return bound;
}

TeaObjectClass* tea_new_class(TeaState* T, TeaObjectString* name, TeaObjectClass* superclass)
{
    TeaObjectClass* klass = ALLOCATE_OBJECT(T, TeaObjectClass, OBJ_CLASS);
    klass->name = name;
    klass->super = superclass;
    klass->constructor = NULL_VAL;
    tea_init_table(&klass->statics);
    tea_init_table(&klass->methods);

    return klass;
}

TeaObjectClosure* tea_new_closure(TeaState* T, TeaObjectFunction* function)
{
    TeaObjectUpvalue** upvalues = ALLOCATE(T, TeaObjectUpvalue*, function->upvalue_count);
    for(int i = 0; i < function->upvalue_count; i++)
    {
        upvalues[i] = NULL;
    }

    TeaObjectClosure* closure = ALLOCATE_OBJECT(T, TeaObjectClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;

    return closure;
}

TeaObjectFunction* tea_new_function(TeaState* T, TeaFunctionType type, TeaObjectModule* module, int max_slots)
{
    TeaObjectFunction* function = ALLOCATE_OBJECT(T, TeaObjectFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->arity_optional = 0;
    function->variadic = 0;
    function->upvalue_count = 0;
    function->max_slots = max_slots;
    function->type = type;
    function->name = NULL;
    function->module = module;
    tea_init_chunk(&function->chunk);

    return function;
}

TeaObjectInstance* tea_new_instance(TeaState* T, TeaObjectClass* klass)
{
    TeaObjectInstance* instance = ALLOCATE_OBJECT(T, TeaObjectInstance, OBJ_INSTANCE);
    instance->klass = klass;
    tea_init_table(&instance->fields);

    return instance;
}

static TeaObjectString* allocate_string(TeaState* T, char* chars, int length, uint32_t hash)
{
    TeaObjectString* string = ALLOCATE_OBJECT(T, TeaObjectString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tea_push_slot(T, OBJECT_VAL(string));
    tea_table_set(T, &T->strings, string, NULL_VAL);
    tea_pop_slot(T);

    return string;
}

static uint32_t hash_string(const char* key, int length)
{
    uint32_t hash = 2166136261u;
    for(int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

TeaObjectString* tea_new_string(TeaState* T, const char* name)
{
    return tea_copy_string(T, name, strlen(name));
}

TeaObjectString* tea_take_string(TeaState* T, char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    TeaObjectString* interned = tea_table_find_string(&T->strings, chars, length, hash);
    if(interned != NULL)
    {
        FREE_ARRAY(T, char, chars, length + 1);
        return interned;
    }

    return allocate_string(T, chars, length, hash);
}

TeaObjectString* tea_copy_string(TeaState* T, const char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    TeaObjectString* interned = tea_table_find_string(&T->strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    char* heap_chars = ALLOCATE(T, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return allocate_string(T, heap_chars, length, hash);
}

TeaObjectUpvalue* tea_new_upvalue(TeaState* T, TeaValue* slot)
{
    TeaObjectUpvalue* upvalue = ALLOCATE_OBJECT(T, TeaObjectUpvalue, OBJ_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;

    return upvalue;
}

static inline uint32_t hash_bits(uint64_t hash)
{
    // From v8's ComputeLongHash() which in turn cites:
    // Thomas Wang, Integer Hash Functions.
    // http://www.concentric.net/~Ttwang/tech/inthash.htm
    hash = ~hash + (hash << 18);  // hash = (hash << 18) - hash - 1;
    hash = hash ^ (hash >> 31);
    hash = hash * 21;  // hash = (hash + (hash << 2)) + (hash << 4);
    hash = hash ^ (hash >> 11);
    hash = hash + (hash << 6);
    hash = hash ^ (hash >> 22);
    return (uint32_t)(hash & 0x3fffffff);
}

static inline uint32_t hash_number(double number)
{
    return hash_bits(num_to_value(number));
}

static uint32_t hash_object(TeaObject* object)
{
    switch(object->type)
    {
        case OBJ_STRING:
            return ((TeaObjectString*)object)->hash;

        default: return 0;
    }
}

static uint32_t hash_value(TeaValue value)
{
#ifdef TEA_NAN_TAGGING
    if(IS_OBJECT(value)) return hash_object(AS_OBJECT(value));

    // Hash the raw bits of the unboxed value.
    return hash_bits(value);
#else
    switch(value.type)
    {
        case VAL_FALSE: return 0;
        case VAL_NULL:  return 1;
        case VAL_NUM:   return hash_number(AS_NUMBER(value));
        case VAL_TRUE:  return 2;
        case VAL_OBJ:   return hash_object(AS_OBJECT(value));
        default:;
    }
    return 0;
#endif
}

static TeaMapItem* find_entry(TeaMapItem* items, int capacity, TeaValue key)
{
    uint32_t hash = hash_value(key);
    uint32_t index = hash & (capacity - 1);
    TeaMapItem* tombstone = NULL;

    while(true)
    {
        TeaMapItem* item = &items[index];
        if(item->empty)
        {
            if(IS_NULL(item->value))
            {
                // Empty item.
                return tombstone != NULL ? tombstone : item;
            }
            else
            {
                // We found a tombstone.
                if(tombstone == NULL)
                    tombstone = item;
            }
        }
        else if(item->key == key)
        {
            // We found the key.
            return item;
        }

        index = (index + 1) & (capacity - 1);
    }
}

bool tea_map_get(TeaObjectMap* map, TeaValue key, TeaValue* value)
{
    if(map->count == 0)
        return false;

    TeaMapItem* item = find_entry(map->items, map->capacity, key);
    if(item->empty)
        return false;

    *value = item->value;

    return true;
}

#define MAP_MAX_LOAD 0.75

bool tea_map_set(TeaState* T, TeaObjectMap* map, TeaValue key, TeaValue value)
{
    if(map->count + 1 > map->capacity * MAP_MAX_LOAD)
    {
        int capacity = GROW_CAPACITY(map->capacity);
        TeaMapItem* items = ALLOCATE(T, TeaMapItem, capacity);
        for(int i = 0; i < capacity; i++)
        {
            items[i].key = NULL_VAL;
            items[i].value = NULL_VAL;
            items[i].empty = true;
        }

        map->count = 0;
        for(int i = 0; i < map->capacity; i++)
        {
            TeaMapItem* item = &map->items[i];
            if(item->empty)
                continue;

            TeaMapItem* dest = find_entry(items, capacity, item->key);
            dest->key = item->key;
            dest->value = item->value;
            dest->empty = false;
            map->count++;
        }

        FREE_ARRAY(T, TeaMapItem, map->items, map->capacity);
        map->items = items;
        map->capacity = capacity;
    }

    TeaMapItem* item = find_entry(map->items, map->capacity, key);
    bool is_new_key = item->empty;

    if(is_new_key && IS_NULL(item->value))
        map->count++;

    item->key = key;
    item->value = value;
    item->empty = false;
    
    return is_new_key;
}

bool tea_map_delete(TeaObjectMap* map, TeaValue key)
{
    if(map->count == 0)
        return false;

    // Find the entry.
    TeaMapItem* item = find_entry(map->items, map->capacity, key);
    if(item->empty)
        return false;

    // Place a tombstone in the entry.
    item->key = NULL_VAL;
    item->value = BOOL_VAL(true);
    item->empty = true;

    return true;
}

void tea_map_add_all(TeaState* T, TeaObjectMap* from, TeaObjectMap* to)
{
    for(int i = 0; i < from->capacity; i++)
    {
        TeaMapItem* item = &from->items[i];
        if(!item->empty)
        {
            tea_map_set(T, to, item->key, item->value);
        }
    }
}

static TeaObjectString* function_tostring(TeaState* T, TeaObjectFunction* function)
{
    if(function->name == NULL)
        return tea_copy_string(T, "<script>", 8);

    return tea_copy_string(T, "<function>", 10);
}

static TeaObjectString* list_tostring(TeaState* T, TeaObjectList* list)
{
    if(list->items.count == 0)
        return tea_copy_string(T, "[]", 2);

    int size = 50;
    
    char* string = ALLOCATE(T, char, size);
    memcpy(string, "[", 1);
    int length = 1;

    for(int i = 0; i < list->items.count; i++) 
    {
        TeaValue value = list->items.values[i];

        char* element;
        int element_size;

        if(value == OBJECT_VAL(list))
        {
            element = "[...]";
            element_size = 5;
        }
        else
        {
            TeaObjectString* s = tea_value_tostring(T, value);
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

            string = GROW_ARRAY(T, char, string, old_size, size);
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

    string = GROW_ARRAY(T, char, string, size, length + 1);

    return tea_take_string(T, string, length);
}

static TeaObjectString* map_tostring(TeaState* T, TeaObjectMap* map)
{
    if(map->count == 0)
        return tea_copy_string(T, "{}", 2);
        
    int count = 0;
    int size = 50;

    char* string = ALLOCATE(T, char, size);
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
        
        if(item->key == OBJECT_VAL(map))
        {
            key = "{...}";
            key_size = 5;
        }
        else
        {
            TeaObjectString* s = tea_value_tostring(T, item->key);
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

            string = GROW_ARRAY(T, char, string, old_size, size);
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
        
        if(item->value == OBJECT_VAL(map))
        {
            element = "{...}";
            element_size = 5;
        }
        else
        {
            TeaObjectString* s = tea_value_tostring(T, item->value);
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

            string = GROW_ARRAY(T, char, string, old_size, size);
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

    string = GROW_ARRAY(T, char, string, size, length + 1);

    return tea_take_string(T, string, length);
}

static TeaObjectString* range_tostring(TeaState* T, TeaObjectRange* range)
{
    char* start = tea_number_tostring(T, range->start)->chars;
    char* end = tea_number_tostring(T, range->end)->chars;

    int len = snprintf(NULL, 0, "%s...%s", start, end);
    char* string = ALLOCATE(T, char, len + 1);
    snprintf(string, len + 1, "%s...%s", start, end);

    return tea_take_string(T, string, len);
}

static TeaObjectString* module_tostring(TeaState* T, TeaObjectModule* module)
{
    int len = snprintf(NULL, 0, "<%s module>", module->name->chars);
    char* string = ALLOCATE(T, char, len + 1);
    snprintf(string, len + 1, "<%s module>", module->name->chars);

    return tea_take_string(T, string, len);
}

static TeaObjectString* class_tostring(TeaState* T, TeaObjectClass* klass)
{
    int len = snprintf(NULL, 0, "<%s>", klass->name->chars);
    char* string = ALLOCATE(T, char, len + 1);
    snprintf(string, len + 1, "<%s>", klass->name->chars);

    return tea_take_string(T, string, len);
}

static TeaObjectString* instance_tostring(TeaState* T, TeaObjectInstance* instance)
{
    int len = snprintf(NULL, 0, "<%s instance>", instance->klass->name->chars);
    char* string = ALLOCATE(T, char, len + 1);
    snprintf(string, len + 1, "<%s instance>", instance->klass->name->chars);

    return tea_take_string(T, string, len);
}

TeaObjectString* tea_object_tostring(TeaState* T, TeaValue value)
{
    switch(OBJECT_TYPE(value))
    {
        case OBJ_FILE:
            return tea_copy_string(T, "<file>", 6);
        case OBJ_USERDATA:
            return tea_copy_string(T, "<userdata>", 10);
        case OBJ_BOUND_METHOD:
            return tea_copy_string(T, "<method>", 8);
        case OBJ_NATIVE:
        {
            switch(AS_NATIVE(value)->type)
            {
                case NATIVE_PROPERTY:
                    return tea_copy_string(T, "<property>", 10);
                case NATIVE_FUNCTION: 
                case NATIVE_METHOD: 
                    return tea_copy_string(T, "<function>", 10);
            }
        }
        case OBJ_FUNCTION:
            return function_tostring(T, AS_FUNCTION(value));
        case OBJ_CLOSURE:
            return function_tostring(T, AS_CLOSURE(value)->function);
        case OBJ_LIST:
            return list_tostring(T, AS_LIST(value));
        case OBJ_MAP:
            return map_tostring(T, AS_MAP(value));
        case OBJ_RANGE:
            return range_tostring(T, AS_RANGE(value));
        case OBJ_MODULE:
            return module_tostring(T, AS_MODULE(value));
        case OBJ_CLASS:
            return class_tostring(T, AS_CLASS(value));
        case OBJ_INSTANCE:
            return instance_tostring(T, AS_INSTANCE(value));
        case OBJ_STRING:
            return AS_STRING(value);
        case OBJ_UPVALUE:
            return tea_copy_string(T, "<upvalue>", 9);
        default:
            break;
    }
    return tea_copy_string(T, "unknown", 7);
}

static bool range_equals(TeaObjectRange* a, TeaObjectRange* b)
{
    return a->start == b->start && a->end == b->end && a->step == b->step;
}

static bool list_equals(TeaObjectList* a, TeaObjectList* b)
{
    if(a->items.count != b->items.count)
    {
        return false;
    }

    for(int i = 0; i < a->items.count; ++i)
    {
        if(!tea_values_equal(a->items.values[i], b->items.values[i]))
        {
            return false;
        }
    }

    return true;
}

static bool map_equals(TeaObjectMap* a, TeaObjectMap* b)
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

        if(!tea_values_equal(item->value, value))
        {
            return false;
        }
    }

    return true;
}

bool tea_objects_equal(TeaValue a, TeaValue b)
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

const char* tea_object_type(TeaValue a)
{
    switch(OBJECT_TYPE(a))
    {
        case OBJ_USERDATA:
            return "userdata";
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
        case OBJ_THREAD:
            return "thread";
        default:
            break;
    }
    return "unknown";
}