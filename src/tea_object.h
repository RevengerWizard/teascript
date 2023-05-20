// tea_object.h
// Teascript object model and functions

#ifndef TEA_OBJECT_H
#define TEA_OBJECT_H

#include <stdio.h>

#include "tea.h"
#include "tea_common.h"
#include "tea_memory.h"
#include "tea_chunk.h"
#include "tea_table.h"
#include "tea_value.h"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_NATIVE(value) teaO_is_object_type(value, OBJ_NATIVE)
#define IS_RANGE(value) teaO_is_object_type(value, OBJ_RANGE)
#define IS_FILE(value) teaO_is_object_type(value, OBJ_FILE)
#define IS_MODULE(value) teaO_is_object_type(value, OBJ_MODULE)
#define IS_LIST(value) teaO_is_object_type(value, OBJ_LIST)
#define IS_MAP(value) teaO_is_object_type(value, OBJ_MAP)
#define IS_BOUND_METHOD(value) teaO_is_object_type(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value) teaO_is_object_type(value, OBJ_CLASS)
#define IS_CLOSURE(value) teaO_is_object_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) teaO_is_object_type(value, OBJ_FUNCTION)
#define IS_INSTANCE(value) teaO_is_object_type(value, OBJ_INSTANCE)
#define IS_STRING(value) teaO_is_object_type(value, OBJ_STRING)

#define AS_NATIVE(value) ((TeaObjectNative*)AS_OBJECT(value))
#define AS_RANGE(value) ((TeaObjectRange*)AS_OBJECT(value))
#define AS_FILE(value) ((TeaObjectFile*)AS_OBJECT(value))
#define AS_MODULE(value) ((TeaObjectModule*)AS_OBJECT(value))
#define AS_LIST(value) ((TeaObjectList*)AS_OBJECT(value))
#define AS_MAP(value) ((TeaObjectMap*)AS_OBJECT(value))
#define AS_BOUND_METHOD(value) ((TeaObjectBoundMethod*)AS_OBJECT(value))
#define AS_CLASS(value) ((TeaObjectClass*)AS_OBJECT(value))
#define AS_CLOSURE(value) ((TeaObjectClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value) ((TeaObjectFunction*)AS_OBJECT(value))
#define AS_INSTANCE(value) ((TeaObjectInstance*)AS_OBJECT(value))
#define AS_STRING(value) ((TeaObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((TeaObjectString*)AS_OBJECT(value))->chars)

#define IS_NATIVE_FUNCTION(value) ((IS_NATIVE(value)) && AS_NATIVE(value)->type == NATIVE_FUNCTION)
#define IS_NATIVE_METHOD(value) ((IS_NATIVE(value)) && AS_NATIVE(value)->type == NATIVE_METHOD)
#define IS_NATIVE_PROPERTY(value) ((IS_NATIVE(value)) && AS_NATIVE(value)->type == NATIVE_PROPERTY)

#define ALLOCATE_OBJECT(T, type, object_type) (type*)teaO_allocate(T, sizeof(type), object_type)

typedef enum
{
    OBJ_STRING,
    OBJ_RANGE,
    OBJ_FUNCTION,
    OBJ_NATIVE,
    OBJ_MODULE,
    OBJ_CLOSURE,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_BOUND_METHOD,
    OBJ_LIST,
    OBJ_MAP,
    OBJ_FILE,
} TeaObjectType;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_CONSTRUCTOR,
    TYPE_STATIC,
    TYPE_METHOD,
    TYPE_SCRIPT
} TeaFunctionType;

struct TeaObject
{
    TeaObjectType type;
    bool is_marked;
    struct TeaObject* next;
};

typedef struct
{
    TeaObject obj;
    double start;
    double end;
    double step;
} TeaObjectRange;

struct TeaObjectFile
{
    TeaObject obj;
    FILE* file;
    TeaObjectString* path;
    TeaObjectString* type;
    int is_open;
};

typedef struct
{
    TeaObject obj;
    TeaObjectString* name;
    TeaObjectString* path;
    TeaTable values;
} TeaObjectModule;

typedef struct
{
    TeaObject obj;
    int arity;
    int arity_optional;
    int variadic;
    int upvalue_count;
    int max_slots;
    TeaChunk chunk;
    TeaFunctionType type;
    TeaObjectString* name;
    TeaObjectModule* module;
} TeaObjectFunction;

typedef enum
{
    NATIVE_FUNCTION,
    NATIVE_METHOD,
    NATIVE_PROPERTY
} TeaNativeType;

typedef struct
{
    TeaObject obj;
    TeaNativeType type;
    TeaCFunction fn;
} TeaObjectNative;

struct TeaObjectString
{
    TeaObject obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct
{
    TeaObject obj;
    TeaValueArray items;
} TeaObjectList;

typedef struct 
{
    TeaValue key;
    TeaValue value;
    bool empty;
} TeaMapItem;

typedef struct
{
    TeaObject obj;
    int count;
    int capacity;
    TeaMapItem* items;
} TeaObjectMap;

typedef struct TeaObjectUpvalue
{
    TeaObject obj;
    TeaValue* location;
    TeaValue closed;
    struct TeaObjectUpvalue* next;
} TeaObjectUpvalue;

typedef struct
{
    TeaObject obj;
    TeaObjectFunction* function;
    TeaObjectUpvalue** upvalues;
    int upvalue_count;
} TeaObjectClosure;

typedef struct TeaObjectClass
{
    TeaObject obj;
    TeaObjectString* name;
    struct TeaObjectClass* super;
    TeaValue constructor;
    TeaTable statics;
    TeaTable methods;
} TeaObjectClass;

typedef struct
{
    TeaObject obj;
    TeaObjectClass* klass;
    TeaTable fields;
} TeaObjectInstance;

typedef struct
{
    TeaObject obj;
    TeaValue receiver;
    TeaValue method;
} TeaObjectBoundMethod;

TeaObjectBoundMethod* teaO_new_bound_method(TeaState* T, TeaValue receiver, TeaValue method);
TeaObjectInstance* teaO_new_instance(TeaState* T, TeaObjectClass* klass);
TeaObjectClass* teaO_new_class(TeaState* T, TeaObjectString* name, TeaObjectClass* superclass);
TeaObjectClosure* teaO_new_closure(TeaState* T, TeaObjectFunction* function);
TeaObjectUpvalue* teaO_new_upvalue(TeaState* T, TeaValue* slot);

TeaObjectMap* teaO_new_map(TeaState* T);
bool teaO_map_set(TeaState* T, TeaObjectMap* map, TeaValue key, TeaValue value);
bool teaO_map_get(TeaObjectMap* map, TeaValue key, TeaValue* value);
bool teaO_map_delete(TeaObjectMap* map, TeaValue key);
void teaO_map_add_all(TeaState* T, TeaObjectMap* from, TeaObjectMap* to);

TeaObjectList* teaO_new_list(TeaState* T);

#define teaO_new_literal(T, s) (teaO_copy_string(T, "" s, (sizeof(s)/sizeof(char))-1))
#define teaO_new_string(T, s) (teaO_copy_string(T, s, strlen(s)))

TeaObjectString* teaO_take_string(TeaState* T, char* chars, int length);
TeaObjectString* teaO_copy_string(TeaState* T, const char* chars, int length);

TeaObjectNative* teaO_new_native(TeaState* T, TeaNativeType type, TeaCFunction fn);
TeaObjectFunction* teaO_new_function(TeaState* T, TeaFunctionType type, TeaObjectModule* module, int max_slots);
TeaObjectModule* teaO_new_module(TeaState* T, TeaObjectString* name);
TeaObjectFile* teaO_new_file(TeaState* T, TeaObjectString* path, TeaObjectString* type);
TeaObjectRange* teaO_new_range(TeaState* T, double start, double end, double step);

TeaObject* teaO_allocate(TeaState* T, size_t size, TeaObjectType type);

TeaObjectString* teaO_tostring(TeaState* T, TeaValue value);
bool teaO_equal(TeaValue a, TeaValue b);
const char* teaO_type(TeaValue a);

static inline bool teaO_is_object_type(TeaValue value, TeaObjectType type)
{
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

static inline bool teaO_is_valid_key(TeaValue value)
{
    return IS_NULL(value) || IS_BOOL(value) || IS_NUMBER(value) ||
    IS_STRING(value);
}

static inline bool teaO_is_falsey(TeaValue value)
{
    return  IS_NULL(value) || 
            (IS_BOOL(value) && !AS_BOOL(value)) || 
            (IS_NUMBER(value) && AS_NUMBER(value) == 0) || 
            (IS_STRING(value) && AS_CSTRING(value)[0] == '\0') || 
            (IS_LIST(value) && AS_LIST(value)->items.count == 0) ||
            (IS_MAP(value) && AS_MAP(value)->count == 0);
}

#endif