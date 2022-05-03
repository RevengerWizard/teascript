#ifndef TEA_OBJECT_H
#define TEA_OBJECT_H

#include <stdio.h>

#include "tea_common.h"
#include "tea_predefines.h"
#include "tea_memory.h"
#include "tea_chunk.h"
#include "tea_table.h"
#include "tea_value.h"

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_RANGE(value) tea_is_object_type(value, OBJ_RANGE)
#define IS_FILE(value) tea_is_object_type(value, OBJ_FILE)
#define IS_MODULE(value) tea_is_object_type(value, OBJ_MODULE)
#define IS_LIST(value) tea_is_object_type(value, OBJ_LIST)
#define IS_MAP(value) tea_is_object_type(value, OBJ_MAP)
#define IS_BOUND_METHOD(value) tea_is_object_type(value, OBJ_BOUND_METHOD)
#define IS_CLASS(value) tea_is_object_type(value, OBJ_CLASS)
#define IS_CLOSURE(value) tea_is_object_type(value, OBJ_CLOSURE)
#define IS_FUNCTION(value) tea_is_object_type(value, OBJ_FUNCTION)
#define IS_INSTANCE(value) tea_is_object_type(value, OBJ_INSTANCE)
#define IS_NATIVE(value) tea_is_object_type(value, OBJ_NATIVE)
#define IS_STRING(value) tea_is_object_type(value, OBJ_STRING)

#define IS_CALLABLE_FUNCTION(value) tea_is_callable_function(value)

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
#define AS_NATIVE(value) \
    (((TeaObjectNative*)AS_OBJECT(value))->function)
#define AS_STRING(value) ((TeaObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((TeaObjectString*)AS_OBJECT(value))->chars)

#define ALLOCATE_OBJECT(state, type, object_type) (type*)tea_allocate_object(state, sizeof(type), object_type)

typedef enum
{
    OBJ_RANGE,
    OBJ_FILE,
    OBJ_MODULE,
    OBJ_LIST,
    OBJ_MAP,
    OBJ_BOUND_METHOD,
    OBJ_CLASS,
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_INSTANCE,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE
} TeaObjectType;

typedef enum
{
    TYPE_FUNCTION,
    TYPE_CONSTRUCTOR,
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
    double from;
    double to;
    bool inclusive;
} TeaObjectRange;

struct TeaObjectFile
{
    TeaObject obj;
    FILE* file;
    char* path;
    char* type;
    bool is_open;
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
    int upvalue_count;
    TeaChunk chunk;
    TeaObjectString* name;
    TeaObjectModule* module;
} TeaObjectFunction;

typedef TeaValue (*TeaNativeFunction)(TeaVM* vm, int arg_count, TeaValue* args, bool* error);

typedef struct
{
    TeaObject obj;
    TeaNativeFunction function;
} TeaObjectNative;

typedef TeaValue (*TeaNativeImport)(TeaVM* vm);

typedef struct
{
    char* name;
    TeaNativeImport module;
} TeaNativeModule;

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
    TeaObject obj;
    TeaTable items;
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
    TeaValue initializer;
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
    TeaObjectClosure* method;
} TeaObjectBoundMethod;

TeaObject* tea_allocate_object(TeaState* state, size_t size, TeaObjectType type);

TeaObjectRange* tea_new_range(TeaState* state, double from, double to, bool inclusive);
TeaObjectFile* tea_new_file(TeaState* state);
TeaObjectModule* tea_new_module(TeaState* state, TeaObjectString* name);
TeaObjectList* tea_new_list(TeaState* state);
TeaObjectMap* tea_new_map(TeaState* state);
TeaObjectBoundMethod* tea_new_bound_method(TeaState* state, TeaValue receiver, TeaObjectClosure* method);
TeaObjectClass* tea_new_class(TeaState* state, TeaObjectString* name);
TeaObjectClosure* tea_new_closure(TeaState* state, TeaObjectFunction* function);
TeaObjectFunction* tea_new_function(TeaState* state, TeaObjectModule* module);
TeaObjectInstance* tea_new_instance(TeaState* state, TeaObjectClass* klass);
TeaObjectNative* tea_new_native(TeaState* state, TeaNativeFunction function);
TeaObjectUpvalue* tea_new_upvalue(TeaState* state, TeaValue* slot);

TeaObjectString* tea_take_string(TeaState* state, char* chars, int length);
TeaObjectString* tea_copy_string(TeaState* state, const char* chars, int length);

char* tea_object_tostring(TeaValue value);
void tea_print_object(TeaValue value);
bool tea_objects_equal(TeaValue a, TeaValue b);
const char* tea_object_type(TeaValue a);

static inline bool tea_is_object_type(TeaValue value, TeaObjectType type)
{
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

static inline bool tea_is_falsey(TeaValue value)
{
    return  IS_NULL(value) || 
            (IS_BOOL(value) && !AS_BOOL(value)) || 
            (IS_NUMBER(value) && AS_NUMBER(value) == 0) || 
            (IS_STRING(value) && AS_CSTRING(value)[0] == '\0') || 
            (IS_LIST(value) && AS_LIST(value)->items.count == 0) ||
            (IS_MAP(value) && AS_MAP(value)->items.count == 0);
}

static inline bool tea_is_callable_function(TeaValue value)
{
    return  IS_CLOSURE(value) ||
            IS_FUNCTION(value) ||
            IS_NATIVE(value) ||
            IS_BOUND_METHOD(value);
}

#endif