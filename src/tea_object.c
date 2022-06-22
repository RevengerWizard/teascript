#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "tea_memory.h"
#include "tea_object.h"
#include "tea_table.h"
#include "tea_value.h"
#include "tea_vm.h"

TeaObject* tea_allocate_object(TeaState* state, size_t size, TeaObjectType type)
{
    TeaObject* object = (TeaObject*)tea_reallocate(state, NULL, 0, size);
    object->type = type;
    object->is_marked = false;

    object->next = state->vm->objects;
    state->vm->objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif

    return object;
}

TeaObjectData* tea_new_data(TeaState* state, size_t size)
{
    TeaObjectData* userdata = ALLOCATE_OBJECT(state, TeaObjectData, OBJ_DATA);

    if(size > 0)
    {
        userdata->data = tea_reallocate(state, NULL, 0, size);
    }
    else
    {
        userdata->data = NULL;
    }

    userdata->size = size;
    userdata->fn = NULL;

    return userdata;
}

TeaObjectRange* tea_new_range(TeaState* state, double from, double to, bool inclusive)
{
    TeaObjectRange* range = ALLOCATE_OBJECT(state, TeaObjectRange, OBJ_RANGE);

    range->from = from;
    range->to = to;
    range->inclusive = inclusive;

    return range;
}

TeaObjectFile* tea_new_file(TeaState* state)
{
    TeaObjectFile* file = ALLOCATE_OBJECT(state, TeaObjectFile, OBJ_FILE);
    file->is_open = false;

    return file;
}

TeaObjectModule* tea_new_module(TeaState* state, TeaObjectString* name)
{
    TeaValue module_val;
    if(tea_table_get(&state->vm->modules, name, &module_val)) 
    {
        return AS_MODULE(module_val);
    }

    TeaObjectModule* module = ALLOCATE_OBJECT(state, TeaObjectModule, OBJ_MODULE);
    tea_init_table(&module->values);
    module->name = name;
    module->path = NULL;

    tea_table_set(state, &state->vm->modules, name, OBJECT_VAL(module));

    return module;
}

TeaObjectList* tea_new_list(TeaState* state)
{
    TeaObjectList* list = ALLOCATE_OBJECT(state, TeaObjectList, OBJ_LIST);
    tea_init_value_array(&list->items);

    return list;
}

TeaObjectMap* tea_new_map(TeaState* state)
{
    TeaObjectMap* map = ALLOCATE_OBJECT(state, TeaObjectMap, OBJ_MAP);
    map->count = 0;
    map->capacity = 0;
    map->items = NULL;
    
    return map;
}

TeaObjectBoundMethod* tea_new_bound_method(TeaState* state, TeaValue receiver, TeaObjectClosure* method)
{
    TeaObjectBoundMethod* bound = ALLOCATE_OBJECT(state, TeaObjectBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;

    return bound;
}

TeaObjectClass* tea_new_class(TeaState* state, TeaObjectString* name, TeaObjectClass* superclass)
{
    TeaObjectClass* klass = ALLOCATE_OBJECT(state, TeaObjectClass, OBJ_CLASS);
    klass->name = name;
    klass->super = superclass;
    klass->constructor = NULL_VAL;
    tea_init_table(&klass->statics);
    tea_init_table(&klass->methods);

    return klass;
}

TeaObjectClosure* tea_new_closure(TeaState* state, TeaObjectFunction* function)
{
    TeaObjectUpvalue** upvalues = ALLOCATE(state, TeaObjectUpvalue*, function->upvalue_count);
    for(int i = 0; i < function->upvalue_count; i++)
    {
        upvalues[i] = NULL;
    }

    TeaObjectClosure* closure = ALLOCATE_OBJECT(state, TeaObjectClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;

    return closure;
}

TeaObjectFunction* tea_new_function(TeaState* state, TeaFunctionType type, TeaObjectModule* module)
{
    TeaObjectFunction* function = ALLOCATE_OBJECT(state, TeaObjectFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->type = type;
    function->name = NULL;
    function->module = module;
    tea_init_chunk(&function->chunk);

    return function;
}

TeaObjectInstance* tea_new_instance(TeaState* state, TeaObjectClass* klass)
{
    TeaObjectInstance* instance = ALLOCATE_OBJECT(state, TeaObjectInstance, OBJ_INSTANCE);
    instance->klass = klass;
    tea_init_table(&instance->fields);

    return instance;
}

static TeaObjectString* allocate_string(TeaState* state, char* chars, int length, uint32_t hash)
{
    TeaObjectString* string = ALLOCATE_OBJECT(state, TeaObjectString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tea_push(state->vm, OBJECT_VAL(string));
    tea_table_set(state, &state->vm->strings, string, NULL_VAL);
    tea_pop(state->vm);

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

TeaObjectString* tea_take_string(TeaState* state, char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    TeaObjectString* interned = tea_table_find_string(&state->vm->strings, chars, length, hash);
    if(interned != NULL)
    {
        FREE_ARRAY(state, char, chars, length + 1);
        return interned;
    }

    return allocate_string(state, chars, length, hash);
}

TeaObjectString* tea_copy_string(TeaState* state, const char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    TeaObjectString* interned = tea_table_find_string(&state->vm->strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    char* heap_chars = ALLOCATE(state, char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return allocate_string(state, heap_chars, length, hash);
}

TeaObjectUpvalue* tea_new_upvalue(TeaState* state, TeaValue* slot)
{
    TeaObjectUpvalue* upvalue = ALLOCATE_OBJECT(state, TeaObjectUpvalue, OBJ_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;

    return upvalue;
}

TeaObjectNativeFunction* tea_new_native_function(TeaState* state, TeaNativeFunction function)
{
    TeaObjectNativeFunction* native = ALLOCATE_OBJECT(state, TeaObjectNativeFunction, OBJ_NATIVE_FUNCTION);
    native->function = function;

    return native;
}

TeaObjectNativeMethod* tea_new_native_method(TeaState* state, TeaNativeMethod method)
{
    TeaObjectNativeMethod* native = ALLOCATE_OBJECT(state, TeaObjectNativeMethod, OBJ_NATIVE_METHOD);
    native->method = method;

    return native;
}

TeaObjectNativeProperty* tea_new_native_property(TeaState* state, TeaNativeProperty property)
{
    TeaObjectNativeProperty* native = ALLOCATE_OBJECT(state, TeaObjectNativeProperty, OBJ_NATIVE_PROPERTY);
    native->property = property;

    return native;
}

void tea_native_value(TeaVM* vm, TeaTable* table, const char* name, TeaValue value)
{
    TeaObjectString* property = tea_copy_string(vm->state, name, strlen(name));
    tea_push(vm, OBJECT_VAL(property));
    tea_table_set(vm->state, table, property, value);
    tea_pop(vm);
}

void tea_native_function(TeaVM* vm, TeaTable* table, const char* name, TeaNativeFunction function)
{
    TeaObjectNativeFunction* native = tea_new_native_function(vm->state, function);
    tea_push(vm, OBJECT_VAL(native));
    TeaObjectString* string = tea_copy_string(vm->state, name, strlen(name));
    tea_push(vm, OBJECT_VAL(string));
    tea_table_set(vm->state, table, string, OBJECT_VAL(native));
    tea_pop(vm);
    tea_pop(vm);
}

void tea_native_method(TeaVM* vm, TeaTable* table, const char* name, TeaNativeMethod method)
{
    TeaObjectNativeMethod* native = tea_new_native_method(vm->state, method);
    tea_push(vm, OBJECT_VAL(native));
    TeaObjectString* string = tea_copy_string(vm->state, name, strlen(name));
    tea_push(vm, OBJECT_VAL(string));
    tea_table_set(vm->state, table, string, OBJECT_VAL(native));
    tea_pop(vm);
    tea_pop(vm);
}

void tea_native_property(TeaVM* vm, TeaTable* table, const char* name, TeaNativeProperty property)
{
    TeaObjectNativeProperty* native = tea_new_native_property(vm->state, property);
    tea_push(vm, OBJECT_VAL(native));
    TeaObjectString* string = tea_copy_string(vm->state, name, strlen(name));
    tea_push(vm, OBJECT_VAL(string));
    tea_table_set(vm->state, table, string, OBJECT_VAL(native));
    tea_pop(vm);
    tea_pop(vm);
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
    return hash_bits(value_to_num(number));
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
#ifdef NAN_TAGGING
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

bool tea_map_set(TeaState* state, TeaObjectMap* map, TeaValue key, TeaValue value)
{
    if(map->count + 1 > map->capacity * TABLE_MAX_LOAD)
    {
        int capacity = GROW_CAPACITY(map->capacity);
        TeaMapItem* items = ALLOCATE(state, TeaMapItem, capacity);
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

        FREE_ARRAY(state, TeaMapItem, map->items, map->capacity);
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
    item->empty = false;

    return true;
}

void tea_map_add_all(TeaState* state, TeaObjectMap* from, TeaObjectMap* to)
{
    for(int i = 0; i < from->capacity; i++)
    {
        TeaMapItem* item = &from->items[i];
        if(item->empty)
        {
            tea_map_set(state, to, item->key, item->value);
        }
    }
}

bool tea_is_valid_key(TeaValue value)
{
    if(IS_NULL(value) || IS_BOOL(value) || IS_NUMBER(value) ||
    IS_STRING(value))
    {
        return true;
    }

    return false;
}

static char* list_tostring(TeaState* state, TeaObjectList* list)
{
    int size = 50;

    if(list->items.count == 0)
        return "[]";

    char* string = ALLOCATE(state, char, size);
    memcpy(string, "[", 1);
    int length = 1;

    for(int i = 0; i < list->items.count; i++) 
    {
        TeaValue value = list->items.values[i];

        char* element;
        int element_size;

        if(IS_STRING(value))
        {
            TeaObjectString* s = AS_STRING(value);
            element = s->chars;
            element_size = s->length;
        }
        else 
        {
            element = tea_value_tostring(state, value);
            element_size = strlen(element);
        }

        if(element_size > (size - length - 6)) 
        {
            if(element_size > size) 
            {
                size = size + element_size * 2 + 6;
            } 
            else
            {
                size = size * 2 + 6;
            }

            char* new = GROW_ARRAY(state, char, string, element_size, size);

            if(new == NULL) 
            {
                printf("Unable to allocate memory\n");
                exit(71);
            }

            string = new;
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
    string[length + 1] = '\0';

    return string;
}

static char* map_tostring(TeaState* state, TeaObjectMap* map)
{
    int count = 0;
    int size = 50;

    if(map->count == 0)
        return "{}";

    char* string = ALLOCATE(state, char, size);
    memcpy(string, "{", 1);
    int length = 1;

    for(int i = 0; i <= map->capacity; i++) 
    {
        TeaMapItem* item = &map->items[i];
        if(item->empty) 
        {
            continue;
        }

        count++;

        char* key;
        int key_size;

        if(IS_STRING(item->key))
        {
            TeaObjectString* s = AS_STRING(item->key);
            key = s->chars;
            key_size = s->length;
        } 
        else 
        {
            key = tea_value_tostring(state, item->key);
            key_size = strlen(key);
        }

        if(key_size > (size - length - key_size - 4)) 
        {
            if(key_size > size) 
            {
                size += key_size * 2 + 4;
            } 
            else 
            {
                size *= 2 + 4;
            }

            char* new = GROW_ARRAY(state, char, string, key_size, size);

            if(new == NULL)
            {
                printf("Unable to allocate memory\n");
                exit(71);
            }

            string = new;
        }

        memcpy(string + length, key, key_size);
        memcpy(string + length + key_size, " = ", 3);
        length += 3 + key_size;

        char* element;
        int element_size;

        if(IS_STRING(item->value)) 
        {
            TeaObjectString* s = AS_STRING(item->value);
            element = s->chars;
            element_size = s->length;
        } 
        else 
        {
            element = tea_value_tostring(state, item->value);
            element_size = strlen(element);
        }

        if(element_size > (size - length - element_size - 6)) 
        {
            if(element_size > size) 
            {
                size += element_size * 2 + 6;
            }
            else
            {
                size = size * 2 + 6;
            }

            char* new = GROW_ARRAY(state, char, string, element_size, size);

            if(new == NULL)
            {
                printf("Unable to allocate memory\n");
                exit(71);
            }

            string = new;
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
    string[length + 1] = '\0';

    return string;
}

static char* range_tostring(TeaState* state, TeaObjectRange* range)
{
    char* a = tea_number_tostring(state, range->from);
    char* b = tea_number_tostring(state, range->to);
    char* c = range->inclusive ? "..." : "..";

    int length = strlen(a) + strlen(c) + strlen(b) + 1;
    char* string = ALLOCATE(state, char, length);
    snprintf(string, length, "%s%s%s", a, c, b);

    return string;
}

char* tea_object_tostring(TeaState* state, TeaValue value)
{
    switch(OBJECT_TYPE(value))
    {
        case OBJ_FILE:
            return "<file>";
        case OBJ_DATA:
            return "<userdata>";
        case OBJ_FUNCTION:
        case OBJ_CLOSURE:
        case OBJ_BOUND_METHOD:
        case OBJ_NATIVE_FUNCTION:
        case OBJ_NATIVE_METHOD:
            return "<function>";
        case OBJ_LIST:
        {
            return list_tostring(state, AS_LIST(value));
        }
        case OBJ_MAP:
        {
            return map_tostring(state, AS_MAP(value));
        }
        case OBJ_RANGE:
        {
            return range_tostring(state, AS_RANGE(value));
        }
        case OBJ_STRING:
            return AS_STRING(value)->chars;
    }

    return "unknown";
}

static void print_list(TeaObjectList* list)
{
    printf("[");

    for(int i = 0; i < list->items.count - 1; i++)
    {
        tea_print_value(list->items.values[i]);
        printf(", ");
    }
    if(list->items.count != 0)
    {
        tea_print_value(list->items.values[list->items.count - 1]);
    }

    printf("]");
}

static void print_map(TeaObjectMap* map)
{
    bool first = true;
    printf("{");

    for(int i = 0; i < map->capacity; i++)
    {
        if(!(map->items[i].empty))
        {
            if(!first) 
            {
                printf(", ");
            }
            first = false;
            if(IS_STRING(map->items[i].key))
            {
                tea_print_value(map->items[i].key);
            }
            else
            {
                printf("[");
                tea_print_value(map->items[i].key);
                printf("]");
            }

            printf(" = ");
            tea_print_value(map->items[i].value);
        }
    }
    
    printf("}");
}

static void print_range(TeaObjectRange* range)
{
    printf("%.16g", range->from);
    
    range->inclusive ? printf("...") : printf("..");

    printf("%.16g", range->to);
}

static void print_function(TeaObjectFunction* function)
{
    if(function->name == NULL)
    {
        printf("<script>");
        return;
    }

    printf("<function>");
}

void tea_print_object(TeaValue value)
{
    switch(OBJECT_TYPE(value))
    {
        case OBJ_FILE:
            printf("<file>");
            break;
        case OBJ_DATA:
            printf("<userdata>");
            break;
        case OBJ_RANGE:
            print_range(AS_RANGE(value));
            break;
        case OBJ_MODULE:
            printf("<%s module>", AS_MODULE(value)->name->chars);
            break;
        case OBJ_LIST:
            print_list(AS_LIST(value));
            break;
        case OBJ_MAP:
            print_map(AS_MAP(value));
            break;
        case OBJ_BOUND_METHOD:
            print_function(AS_BOUND_METHOD(value)->method->function);
            break;
        case OBJ_CLASS:
            printf("<%s>", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            print_function(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            print_function(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("<%s instance>", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJ_NATIVE_METHOD:
        case OBJ_NATIVE_FUNCTION:
            printf("<function>");
            break;
        case OBJ_STRING:
            fwrite(AS_CSTRING(value), sizeof(char), strlen(AS_CSTRING(value)), stdout);
            break;
        case OBJ_UPVALUE:
            printf("<upvalue>");
            break;
    }
}

static bool range_equals(TeaValue a, TeaValue b)
{
    TeaObjectRange* r1 = AS_RANGE(a);
    TeaObjectRange* r2 = AS_RANGE(b);

    return r1->from == r2->from && r1->to == r2->to && r1->inclusive == r2->inclusive;
}

static bool list_equals(TeaValue a, TeaValue b)
{
    TeaObjectList* l1 = AS_LIST(a);
    TeaObjectList* l2 = AS_LIST(b);

    if(l1->items.count != l2->items.count)
    {
        return false;
    }

    for(int i = 0; i < l1->items.count; ++i)
    {
        if(!tea_values_equal(l1->items.values[i], l2->items.values[i]))
        {
            return false;
        }
    }

    return true;
}

static bool map_equals(TeaValue a, TeaValue b)
{
    TeaObjectMap* m1 = AS_MAP(a);
    TeaObjectMap* m2 = AS_MAP(b);

    if(m1->count != m2->count)
    {
        return false;
    }

    if(m1->count == 0)
    {
        return true;
    }

    for(int i = 0; i < m1->capacity; i++)
    {
        TeaMapItem* item = &m1->items[i];

        if(item->empty)
        {
            continue;
        }

        TeaValue value;
        if(!tea_map_get(m2, item->key, &value))
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
        {
            return range_equals(a, b);
        }
        case OBJ_LIST:
        {
            return list_equals(a, b);
        }
        case OBJ_MAP:
        {
            return map_equals(a, b);
        }
        default:
            break;
    }

    return a == b;
}

const char* tea_object_type(TeaValue a)
{
    switch(OBJECT_TYPE(a))
    {
        case OBJ_DATA:
            return "data";
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
        case OBJ_CLOSURE:
        case OBJ_FUNCTION:
        case OBJ_NATIVE_FUNCTION:
        case OBJ_NATIVE_METHOD:
            return "function";
        default:
            return "unknown";
    }
}