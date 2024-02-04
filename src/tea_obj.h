/*
** tea_obj.h
** Teascript values and objects
*/

#ifndef _TEA_OBJ_H
#define _TEA_OBJ_H

#include <stdio.h>
#include <string.h>

#include "tea.h"

#include "tea_def.h"

#ifdef TEA_NAN_TAGGING

#define SIGN_BIT ((uint64_t)0x8000000000000000)
#define QNAN ((uint64_t)0x7ffc000000000000)

#define MASK_TAG        (7)

#define TAG_NULL        (1)
#define TAG_FALSE       (2)
#define TAG_TRUE        (3)
#define TAG_UNUSED1     (4)
#define TAG_UNUSED2     (5)
#define TAG_UNUSED3     (6)
#define TAG_UNUSED4     (7)

typedef uint64_t Value;

#define IS_BOOL(v)      (((v) | 1) == TRUE_VAL)
#define IS_NULL(v)      ((v) == NULL_VAL)
#define IS_NUMBER(v)    (((v) & QNAN) != QNAN)
#define IS_OBJECT(v) \
    (((v) & (QNAN | SIGN_BIT)) == (QNAN | SIGN_BIT))

#define AS_BOOL(v)      ((v) == TRUE_VAL)
#define AS_NUMBER(v) value2num(v)
#define AS_OBJECT(v) \
    ((GCobj*)(uintptr_t)((v) & ~(SIGN_BIT | QNAN)))

#define BOOL_VAL(b)     ((b) ? TRUE_VAL : FALSE_VAL)
#define FALSE_VAL       ((Value)(uint64_t)(QNAN | TAG_FALSE))
#define TRUE_VAL        ((Value)(uint64_t)(QNAN | TAG_TRUE))
#define NULL_VAL        ((Value)(uint64_t)(QNAN | TAG_NULL))
#define NUMBER_VAL(num) num2value(num)
#define OBJECT_VAL(obj) \
    (Value)(SIGN_BIT | QNAN | (uint64_t)(uintptr_t)(obj))

#define GET_TAG(value)  ((int)((value) & MASK_TAG))

static TEA_INLINE double value2num(Value value)
{
    double num;
    memcpy(&num, &value, sizeof(Value));
    return num;
}

static TEA_INLINE Value num2value(double num)
{
    Value value;
    memcpy(&value, &num, sizeof(double));
    return value;
}

#else

typedef enum
{
    VAL_NULL,
    VAL_BOOL,
    VAL_NUMBER,
    VAL_OBJECT
} ValueType;

typedef struct
{
    uint8_t type;
    union
    {
        bool boolean;
        double number;
        GCobj* obj;
    } as;
} Value;

#define IS_BOOL(v) ((v).type == VAL_BOOL)
#define IS_NULL(v) ((v).type == VAL_NULL)
#define IS_NUMBER(v) ((v).type == VAL_NUMBER)
#define IS_OBJECT(v) ((v).type == VAL_OBJECT)

#define AS_OBJECT(v) ((v).as.obj)
#define AS_BOOL(v) ((v).as.boolean)
#define AS_NUMBER(v) ((v).as.number)

#define BOOL_VAL(v) ((Value){VAL_BOOL, {.boolean = v}})
#define FALSE_VAL ((Value){VAL_BOOL, {.boolean = false}})
#define TRUE_VAL ((Value){VAL_BOOL, {.boolean = true}})
#define NULL_VAL ((Value){VAL_NULL, {.number = 0}})
#define NUMBER_VAL(v) ((Value){VAL_NUMBER, {.number = v}})
#define OBJECT_VAL(obj) ((Value){VAL_OBJECT, {.obj = (GCobj*)obj}})

#endif

#define OBJECT_TYPE(v) (AS_OBJECT(v)->type)

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
    uint8_t type;
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
    Value value;
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
    int count;  /* Number of bytecode instructions */
    int size;
    uint8_t* code;  /* Bytecode instructions */
    int k_size;
    int k_count;    /* Number of constants */
    Value* k;  /* Constants used by the function */
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
    tea_CFunction fn;
    int nargs;
} GCfuncC;

typedef struct
{
    GCobj obj;
    int count;  /* Number of list items */
    int size;
    Value* items;  /* Array of list values */
} GClist;

typedef struct
{
    Value key;
    Value value;
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
    Value* location;
    Value closed;
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
    Value constructor;
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
    Value receiver;
    Value method;
} GCmethod;

TEA_FUNC GCobj* tea_obj_alloc(tea_State* T, size_t size, ObjType type);

TEA_FUNC GCmethod* tea_obj_new_method(tea_State* T, Value receiver, Value method);
TEA_FUNC GCinstance* tea_obj_new_instance(tea_State* T, GCclass* klass);
TEA_FUNC GCclass* tea_obj_new_class(tea_State* T, GCstr* name, GCclass* superclass);

TEA_FUNC GClist* tea_obj_new_list(tea_State* T);
TEA_FUNC void tea_list_append(tea_State* T, GClist* list, Value value);

TEA_FUNC GCmodule* tea_obj_new_module(tea_State* T, GCstr* name);
TEA_FUNC GCfile* tea_obj_new_file(tea_State* T, GCstr* path, GCstr* type);
TEA_FUNC GCrange* tea_obj_new_range(tea_State* T, double start, double end, double step);

TEA_FUNC const char* tea_val_type(Value a);
TEA_FUNC bool tea_val_equal(Value a, Value b);
TEA_FUNC bool tea_val_rawequal(Value a, Value b);
TEA_FUNC double tea_val_tonumber(Value value, bool* x);
TEA_FUNC GCstr* tea_val_tostring(tea_State* T, Value value, int depth);

TEA_DATA const char* const tea_val_typenames[];
TEA_DATA const char* const tea_obj_typenames[];

static TEA_INLINE bool tea_obj_istype(Value value, ObjType type)
{
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

static TEA_INLINE bool tea_obj_isfalse(Value value)
{
    return  IS_NULL(value) ||
            (IS_BOOL(value) && !AS_BOOL(value)) ||
            (IS_NUMBER(value) && AS_NUMBER(value) == 0) ||
            (IS_STRING(value) && AS_CSTRING(value)[0] == '\0') ||
            (IS_LIST(value) && AS_LIST(value)->count == 0) ||
            (IS_MAP(value) && AS_MAP(value)->count == 0);
}

#endif