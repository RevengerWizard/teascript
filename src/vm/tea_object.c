#include <stdio.h>
#include <string.h>

#include "memory/tea_memory.h"
#include "vm/tea_object.h"
#include "util/tea_table.h"
#include "vm/tea_value.h"
#include "vm/tea_vm.h"

#define ALLOCATE_OBJECT(type, object_type) \
    (type*)allocate_object(sizeof(type), object_type)

static TeaObject* allocate_object(size_t size, TeaObjectType type)
{
    TeaObject* object = (TeaObject*)tea_reallocate(NULL, 0, size);
    object->type = type;
    object->is_marked = false;

    object->next = vm.objects;
    vm.objects = object;

#ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void *)object, size, type);
#endif

    return object;
}

TeaObjectRange* tea_new_range(double from, double to, bool inclusive)
{
    TeaObjectRange* range = ALLOCATE_OBJECT(TeaObjectRange, OBJ_RANGE);

    range->from = from;
    range->to = to;
    range->inclusive = inclusive;

    return range;
}

TeaObjectFile* tea_new_file()
{
    return ALLOCATE_OBJECT(TeaObjectFile, OBJ_FILE);
}

TeaObjectModule* tea_new_module(TeaObjectString* name)
{
    TeaValue module_val;
    if(tea_table_get(&vm.modules, name, &module_val)) 
    {
        return AS_MODULE(module_val);
    }

    TeaObjectModule* module = ALLOCATE_OBJECT(TeaObjectModule, OBJ_MODULE);
    tea_init_table(&module->values);
    module->name = name;
    module->path = NULL;

    tea_table_set(&vm.modules, name, OBJECT_VAL(module));

    return module;
}

TeaObjectList* tea_new_list()
{
    TeaObjectList* list = ALLOCATE_OBJECT(TeaObjectList, OBJ_LIST);
    tea_init_value_array(&list->items);

    return list;
}

TeaObjectMap* tea_new_map()
{
    TeaObjectMap* map = ALLOCATE_OBJECT(TeaObjectMap, OBJ_MAP);
    tea_init_table(&map->items);
    
    return map;
}

TeaObjectBoundMethod* tea_new_bound_method(TeaValue receiver, TeaObjectClosure* method)
{
    TeaObjectBoundMethod* bound = ALLOCATE_OBJECT(TeaObjectBoundMethod, OBJ_BOUND_METHOD);
    bound->receiver = receiver;
    bound->method = method;

    return bound;
}

TeaObjectClass* tea_new_class(TeaObjectString* name)
{
    TeaObjectClass* klass = ALLOCATE_OBJECT(TeaObjectClass, OBJ_CLASS);
    klass->name = name;
    klass->initializer = NULL_VAL;
    tea_init_table(&klass->methods);

    return klass;
}

TeaObjectClosure* tea_new_closure(TeaObjectFunction* function)
{
    TeaObjectUpvalue** upvalues = ALLOCATE(TeaObjectUpvalue*,function->upvalue_count);
    for (int i = 0; i < function->upvalue_count; i++)
    {
        upvalues[i] = NULL;
    }

    TeaObjectClosure* closure = ALLOCATE_OBJECT(TeaObjectClosure, OBJ_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalue_count = function->upvalue_count;

    return closure;
}

TeaObjectFunction* tea_new_function()
{
    TeaObjectFunction* function = ALLOCATE_OBJECT(TeaObjectFunction, OBJ_FUNCTION);
    function->arity = 0;
    function->upvalue_count = 0;
    function->name = NULL;
    tea_init_chunk(&function->chunk);

    return function;
}

TeaObjectInstance* tea_new_instance(TeaObjectClass* klass)
{
    TeaObjectInstance* instance = ALLOCATE_OBJECT(TeaObjectInstance, OBJ_INSTANCE);
    instance->klass = klass;
    tea_init_table(&instance->fields);

    return instance;
}

TeaObjectNative* tea_new_native(TeaNativeFunction function)
{
    TeaObjectNative* native = ALLOCATE_OBJECT(TeaObjectNative, OBJ_NATIVE);
    native->function = function;

    return native;
}

static TeaObjectString* allocate_string(char* chars, int length, uint32_t hash)
{
    TeaObjectString* string = ALLOCATE_OBJECT(TeaObjectString, OBJ_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    tea_push(OBJECT_VAL(string));
    tea_table_set(&vm.strings, string, NULL_VAL);
    tea_pop();

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

TeaObjectString* tea_take_string(char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    TeaObjectString* interned = tea_table_find_string(&vm.strings, chars, length, hash);
    if(interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }

    return allocate_string(chars, length, hash);
}

TeaObjectString* tea_copy_string(const char* chars, int length)
{
    uint32_t hash = hash_string(chars, length);
    TeaObjectString* interned = tea_table_find_string(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    char* heap_chars = ALLOCATE(char, length + 1);
    memcpy(heap_chars, chars, length);
    heap_chars[length] = '\0';

    return allocate_string(heap_chars, length, hash);
}

TeaObjectUpvalue* tea_new_upvalue(TeaValue* slot)
{
    TeaObjectUpvalue* upvalue = ALLOCATE_OBJECT(TeaObjectUpvalue, OBJ_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;

    return upvalue;
}

static char* list_tostring(TeaObjectList* list)
{
    return "";
} 

char* tea_object_tostring(TeaValue value)
{
    switch(OBJECT_TYPE(value))
    {
        case OBJ_LIST:
        {
            return "";
        }
        case OBJ_MAP:
        {
            return "";
        }
        case OBJ_STRING:
        {
            return AS_STRING(value)->chars;
        }
    }
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

    for(int i = 0; i < map->items.capacity; i++)
    {
        if(!(map->items.entries[i].key == NULL))
        {
            if(!first) 
            {
                printf(", ");
            }
            first = false;
            tea_print_value(OBJECT_VAL(map->items.entries[i].key));
            printf(" = ");
            tea_print_value(map->items.entries[i].value);
        }
    }
    
    printf("}");
}

static void print_range(TeaObjectRange* range)
{
    printf("%.15g", range->from);
    
    if(range->inclusive)
    {
        printf("...");
    }
    else
    {
        printf("..");
    }

    printf("%.15g", range->to);
}

static void print_function(TeaObjectFunction* function)
{
    if(function->name == NULL)
    {
        printf("<script>");
        return;
    }
    //printf("<function %s>", function->name->chars);
    printf("<function>");
}

void tea_print_object(TeaValue value)
{
    switch(OBJECT_TYPE(value))
    {
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
            printf("%s", AS_CLASS(value)->name->chars);
            break;
        case OBJ_CLOSURE:
            print_function(AS_CLOSURE(value)->function);
            break;
        case OBJ_FUNCTION:
            print_function(AS_FUNCTION(value));
            break;
        case OBJ_INSTANCE:
            printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
            break;
        case OBJ_NATIVE:
            printf("<native function>");
            break;
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
        case OBJ_UPVALUE:
            printf("upvalue");
            break;
    }
}

static bool range_equals(TeaValue a, TeaValue b)
{
    TeaObjectRange* r1 = AS_RANGE(a);
    TeaObjectRange* r2 = AS_RANGE(b);

    if(r1->from == r2->from && r1->to == r2->to && r1->inclusive == r2->inclusive)
    {
        return true;
    }

    return false;
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

    if(m1->items.count != m2->items.count)
    {
        return true;
    }

    if(m1->items.count == 0)
    {
        return true;
    }

    for(int i = 0; i < m1->items.capacity; i++)
    {
        TeaEntry* item = &m1->items.entries[i];

        if(item->key == NULL)
        {
            continue;
        }

        TeaValue value;
        if(!tea_table_get(&m2->items, item->key, &value))
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
        case OBJ_STRING:
        {
            return a == b;
        }
        default:
            break;
    }

    return false;
}

const char* tea_object_type(TeaValue a)
{
    switch(OBJECT_TYPE(a))
    {
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
            return "function";
        case OBJ_NATIVE:
            return "native";
        default:
            return "unknown";
    }
}