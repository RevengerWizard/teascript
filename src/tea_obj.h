/*
** tea_obj.h
** Teascript VM tags, values and objects
*/

#ifndef _TEA_OBJ_H
#define _TEA_OBJ_H

#include <stdio.h>
#include <string.h>

#include "tea.h"

#include "tea_def.h"

enum
{
    VAL_NULL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_POINTER,
    VAL_OBJECT
};

/* Tagged value */
typedef struct
{
    uint8_t tt;
    union
    {
        bool b;
        void* p;
        double n;
        struct GCobj* gc;
    } as;
} TValue;

#define IS_NULL(v) ((v).tt == VAL_NULL)
#define IS_BOOL(v) ((v).tt == VAL_BOOL)
#define IS_NUMBER(v) ((v).tt == VAL_NUMBER)
#define IS_POINTER(v) ((v).tt == VAL_POINTER)
#define IS_OBJECT(v) ((v).tt == VAL_OBJECT)

#define AS_BOOL(v) ((v).as.b)
#define AS_NUMBER(v) ((v).as.n)
#define AS_POINTER(v) ((v).as.p)
#define AS_OBJECT(v) ((v).as.gc)

#define NULL_VAL ((TValue){VAL_NULL, {.n = 0}})
#define BOOL_VAL(v) ((TValue){VAL_BOOL, {.b = v}})
#define FALSE_VAL ((TValue){VAL_BOOL, {.b = false}})
#define TRUE_VAL ((TValue){VAL_BOOL, {.b = true}})
#define NUMBER_VAL(v) ((TValue){VAL_NUMBER, {.n = v}})
#define POINTER_VAL(v) ((TValue){VAL_POINTER, {.p = v}})
#define OBJECT_VAL(o) ((TValue){VAL_OBJECT, {.gc = (GCobj*)o}})

#define OBJECT_TYPE(v) (AS_OBJECT(v)->tt)

#define IS_CFUNC(v) tea_obj_istype(v, OBJ_CFUNC)
#define IS_RANGE(v) tea_obj_istype(v, OBJ_RANGE)
#define IS_FILE(v) tea_obj_istype(v, OBJ_FILE)
#define IS_MODULE(v) tea_obj_istype(v, OBJ_MODULE)
#define IS_LIST(v) tea_obj_istype(v, OBJ_LIST)
#define IS_MAP(v) tea_obj_istype(v, OBJ_MAP)
#define IS_METHOD(v) tea_obj_istype(v, OBJ_METHOD)
#define IS_CLASS(v) tea_obj_istype(v, OBJ_CLASS)
#define IS_FUNC(v) tea_obj_istype(v, OBJ_FUNC)
#define IS_PROTO(v) tea_obj_istype(v, OBJ_PROTO)
#define IS_INSTANCE(v) tea_obj_istype(v, OBJ_INSTANCE)
#define IS_STRING(v) tea_obj_istype(v, OBJ_STRING)

#define AS_CFUNC(v) ((GCfuncC*)AS_OBJECT(v))
#define AS_RANGE(v) ((GCrange*)AS_OBJECT(v))
#define AS_FILE(v) ((GCfile*)AS_OBJECT(v))
#define AS_MODULE(v) ((GCmodule*)AS_OBJECT(v))
#define AS_LIST(v) ((GClist*)AS_OBJECT(v))
#define AS_MAP(v) ((GCmap*)AS_OBJECT(v))
#define AS_METHOD(v) ((GCmethod*)AS_OBJECT(v))
#define AS_CLASS(v) ((GCclass*)AS_OBJECT(v))
#define AS_FUNC(v) ((GCfuncT*)AS_OBJECT(v))
#define AS_PROTO(v) ((GCproto*)AS_OBJECT(v))
#define AS_INSTANCE(v) ((GCinstance*)AS_OBJECT(v))
#define AS_STRING(v) ((GCstr*)AS_OBJECT(v))
#define AS_CSTRING(v) (((GCstr*)AS_OBJECT(v))->chars)

#define tea_obj_new(T, type, object_type) (type*)tea_obj_alloc(T, sizeof(type), object_type)

typedef enum
{
    OBJ_STRING,
    OBJ_RANGE,
    OBJ_PROTO,
    OBJ_CFUNC,
    OBJ_MODULE,
    OBJ_FUNC,
    OBJ_UPVALUE,
    OBJ_CLASS,
    OBJ_INSTANCE,
    OBJ_METHOD,
    OBJ_LIST,
    OBJ_MAP,
    OBJ_FILE,
} ObjType;

typedef struct GCobj
{
    uint8_t tt;
    uint8_t marked;
    struct GCobj* next;
} GCobj;

typedef struct GCstr
{
    GCobj obj;
    int len;    /* Size of string */
    char* chars;    /* Data of string */
    uint32_t hash;  /* Hash of string */
} GCstr;

typedef struct
{
    GCstr* key;
    TValue value;
} TableEntry;

typedef struct
{
    int count;
    int size;
    TableEntry* entries;
} Table;

typedef struct
{
    GCobj obj;
    double start;
    double end;
    double step;
} GCrange;

typedef struct GCfile
{
    GCobj obj;
    FILE* file;
    GCstr* path;
    GCstr* type;
    int is_open;
} GCfile;

typedef struct
{
    GCobj obj;
    GCstr* name;
    GCstr* path;
    Table values;
} GCmodule;

typedef enum
{
    PROTO_FUNCTION,
    PROTO_ANONYMOUS,
    PROTO_CONSTRUCTOR,
    PROTO_STATIC,
    PROTO_METHOD,
    PROTO_OPERATOR,
    PROTO_SCRIPT
} ProtoType;

typedef struct
{
    int offset; /* Bytecode instruction */
    int line;   /* Line number for this bytecode */
} LineStart;

typedef struct
{
    GCobj obj;
    uint8_t arity;  /* Number of arguments */
    uint8_t arity_optional; /* Number of optional arguments */
    uint8_t variadic;   /* Function has variadic argument */
    int upvalue_count;  /* Number of upvalues */
    uint8_t max_slots;  /* Max stack size used by the function */
    uint8_t type;   /* Function type information */
    int bc_count;  /* Number of bytecode instructions */
    int bc_size;
    uint8_t* bc;  /* Bytecode instructions */
    int k_size;
    int k_count;    /* Number of constants */
    TValue* k;  /* Constants used by the function */
    int line_count; /* Number of lines for the function definition */
    int line_size;
    LineStart* lines;   /* Map from bytecode ins. to source lines */
    GCstr* name;    /* Name of the function */
    GCmodule* module;  /* Module namespace for the function */
} GCproto;

typedef enum
{
    C_FUNCTION,
    C_METHOD,
    C_PROPERTY
} CFuncType;

typedef struct
{
    GCobj obj;
    uint8_t type;
    tea_CFunction fn;   /* C function to be called */
    int nargs;
} GCfuncC;

typedef struct
{
    GCobj obj;
    int count;  /* Number of list items */
    int size;
    TValue* items;  /* Array of list values */
} GClist;

typedef struct
{
    TValue key;
    TValue value;
    bool empty;
} MapEntry;

typedef struct
{
    GCobj obj;
    int count;  /* Number of map fields */
    int size;
    MapEntry* entries;
} GCmap;

typedef struct GCupvalue
{
    GCobj obj;
    TValue* location;
    TValue closed;
    struct GCupvalue* next;
} GCupvalue;

typedef struct
{
    GCobj obj;
    GCproto* proto;
    GCupvalue** upvalues;
    int upvalue_count;
} GCfuncT;

typedef struct GCclass
{
    GCobj obj;
    GCstr* name;
    struct GCclass* super;
    TValue constructor;
    Table statics;
    Table methods;
} GCclass;

typedef struct
{
    GCobj obj;
    GCclass* klass;
    Table fields;
} GCinstance;

typedef struct
{
    GCobj obj;
    TValue receiver;
    TValue method;
} GCmethod;

TEA_FUNC GCobj* tea_obj_alloc(tea_State* T, size_t size, ObjType type);

TEA_FUNC GCmethod* tea_obj_new_method(tea_State* T, TValue receiver, TValue method);
TEA_FUNC GCinstance* tea_obj_new_instance(tea_State* T, GCclass* klass);
TEA_FUNC GCclass* tea_obj_new_class(tea_State* T, GCstr* name, GCclass* superclass);

TEA_FUNC GClist* tea_obj_new_list(tea_State* T);
TEA_FUNC void tea_list_append(tea_State* T, GClist* list, TValue value);

TEA_FUNC GCmodule* tea_obj_new_module(tea_State* T, GCstr* name);
TEA_FUNC GCfile* tea_obj_new_file(tea_State* T, GCstr* path, GCstr* type);
TEA_FUNC GCrange* tea_obj_new_range(tea_State* T, double start, double end, double step);

TEA_FUNC const char* tea_val_type(TValue a);
TEA_FUNC bool tea_val_equal(TValue a, TValue b);
TEA_FUNC bool tea_val_rawequal(TValue a, TValue b);
TEA_FUNC double tea_val_tonumber(TValue value, bool* x);
TEA_FUNC GCstr* tea_val_tostring(tea_State* T, TValue value, int depth);

TEA_DATA const char* const tea_val_typenames[];
TEA_DATA const char* const tea_obj_typenames[];

static TEA_AINLINE bool tea_obj_istype(TValue value, ObjType type)
{
    return IS_OBJECT(value) && AS_OBJECT(value)->tt == type;
}

static TEA_AINLINE bool tea_obj_isfalse(TValue value)
{
    return  IS_NULL(value) ||
            (IS_BOOL(value) && !AS_BOOL(value)) ||
            (IS_NUMBER(value) && AS_NUMBER(value) == 0) ||
            (IS_STRING(value) && AS_CSTRING(value)[0] == '\0') ||
            (IS_LIST(value) && AS_LIST(value)->count == 0) ||
            (IS_MAP(value) && AS_MAP(value)->count == 0);
}

#endif