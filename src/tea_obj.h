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

/* -- Tags and values ----------------------------------------------------- */

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
    } value;
} TValue;

/* Internal object tags */
enum
{
    TEA_TNULL,
    TEA_TBOOL,
    TEA_TNUMBER,
    TEA_TPOINTER,
    TEA_TSTRING,
    TEA_TRANGE,
    TEA_TFUNC,
    TEA_TCFUNC,
    TEA_TMODULE,
    TEA_TCLASS,
    TEA_TINSTANCE,
    TEA_TMETHOD,
    TEA_TLIST,
    TEA_TMAP,
    TEA_TFILE,
    TEA_TPROTO,
    TEA_TUPVALUE,
};

/* GC common header */
typedef struct GCobj
{
    uint8_t gct;
    uint8_t marked;
    struct GCobj* next;
} GCobj;

/* Resizable string buffer */
typedef struct SBuf
{
    char* w;    /* Write pointer */
    char* e;    /* End pointer */
    char* b;    /* Base pointer */
} SBuf;

/* -- String object -------------------------------------------------- */

typedef struct GCstr
{
    GCobj obj;
    int len;    /* Size of string */
    char* chars;    /* Data of string */
    uint32_t hash;  /* Hash of string */
} GCstr;

/* -- Hash table -------------------------------------------------- */

/* Hash node */
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

/* -- Range object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    double start;
    double end;
    double step;
} GCrange;

/* -- File object -------------------------------------------------- */

typedef struct GCfile
{
    GCobj obj;
    FILE* file;
    GCstr* path;
    GCstr* type;
    int is_open;
} GCfile;

/* -- Module object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    GCstr* name;    /* Canonical module name */
    GCstr* path;    /* Absolute module path */
    Table values;   /* Table of defined variables */
} GCmodule;

/* -- Prototype object -------------------------------------------------- */

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

/* -- Upvalue object -------------------------------------------------- */

typedef struct GCupvalue
{
    GCobj obj;
    TValue* location;
    TValue closed;
    struct GCupvalue* next;
} GCupvalue;

/* -- Function object (closures) -------------------------------------------------- */

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
    int nargs;  /* Number of arguments or -1 */
} GCfuncC;

typedef struct
{
    GCobj obj;
    GCproto* proto;
    GCupvalue** upvalues;
    int upvalue_count;
} GCfuncT;

/* -- List object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    int count;  /* Number of list items */
    int size;
    TValue* items;  /* Array of list values */
} GClist;

/* -- Map object -------------------------------------------------- */

/* Map node */
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

/* -- Class objects -------------------------------------------------- */

typedef struct GCclass
{
    GCobj obj;
    GCstr* name;
    struct GCclass* super;
    TValue constructor; /* Cached */
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

/* -- State objects -------------------------------------------------- */

/* Information about a call */
typedef struct
{
    GCfuncT* func;
    GCfuncC* cfunc;
    uint8_t* ip;
    int state;
    TValue* base; /* Base for this function */
} CallInfo;

/* Flags for CallInfo state */
#define CIST_C    (1 << 0)  /* Call is running a C function */
#define CIST_REENTRY  (1 << 1)  /* Call is running on a new invocation of 'vm_execute' */
#define CIST_CALLING  (1 << 2)  /* Call a Teascript function */
#define CIST_TEA   (1 << 3) /* Call is running a Teascript function */

/* Special methods */
#define MMDEF(_) \
    _(PLUS, +) _(MINUS, -) _(MULT, *) _(DIV, /) \
    _(MOD, %) _(POW, **) _(BAND, &) _(BOR, |) _(BNOT, ~) \
    _(BXOR, ^) _(LSHIFT, <<) _(RSHIFT, >>) \
    _(LT, <) _(LE, <=) _(GT, >) _(GE, >=) _(EQ, ==) \
    _(INDEX, []) _(TOSTRING, tostring) \
    _(ITER, iterate) _(NEXT, iteratorvalue) \
    _(CONTAINS, contains) _(GC, gc)

typedef enum
{
#define MMENUM(name, _) MM_##name,
MMDEF(MMENUM)
#undef MMENUM
    MM__MAX
} MMS;

/* Garbage collector state */
typedef struct GCState
{
    GCobj* objects;    /* List of all collectable objects */
    size_t bytes_allocated; /* Number of bytes currently allocated */
    size_t next_gc; /* Number of bytes to activate next GC */
    int gray_count; /* Number of grayed GC objects */
    int gray_size;
    GCobj** gray_stack; /* List of gray objects */
} GCState;

/* Per interpreter state */
struct tea_State
{
    TValue* stack_max;   /* Last free slot in the stack */
    TValue* stack;    /* Stack base */
    TValue* top;  /* First free slot in the stack */
    TValue* base; /* Base of current function */
    int stack_size; 
    CallInfo* ci;    /* CallInfo for current function */
    CallInfo* ci_end;    /* Points after end of ci array */
    CallInfo* ci_base;   /* CallInfo base */
    int ci_size;    /* Size of array 'ci_base' */
    GCupvalue* open_upvalues; /* List of open upvalues in the stack */
    struct tea_longjmp* error_jump; /* Current error recovery point */
    int nccalls;    /* Number of nested C calls */
    /* ------ The following fields are global to the state ------ */
    GCState gc; /* Garbage collector */
    struct Parser* parser;
    Table modules;   /* Table of cached modules */
    Table globals;   /* Table of globals */
    Table constants;    /* Table to keep track of 'const' variables */
    Table strings;   /* String interning */
    SBuf tmpbuf;    /* Termorary string buffer */
    GCmodule* last_module;    /* Last cached module */
    GCclass* number_class;
    GCclass* bool_class;
    GCclass* string_class;
    GCclass* list_class;
    GCclass* map_class;
    GCclass* file_class;
    GCclass* range_class;
    GCstr* constructor_string;  /* "constructor" */
    GCstr* repl_string; /* "_" */
    GCstr* memerr;  /* String message for out-of-memory situation */
    GCstr* opm_name[MM__MAX];   /* Array with special method names  */
    tea_CFunction panic; /* Function to be called in unprotected errors */
    tea_Alloc allocf;  /* Memory allocator */
    void* allocd;   /* Memory allocator data */
    int argc;
    char** argv;
    int argf;
    bool repl;
};

/* -- TValue getters/setters -------------------------------------------------- */

/* Macros to test types */
#define itype(o) ((o)->tt)
#define tvisnull(o) (itype(o) == TEA_TNULL)
#define tvisbool(o) (itype(o) == TEA_TBOOL)
#define tvisnumber(o) (itype(o) == TEA_TNUMBER)
#define tvispointer(o) (itype(o) == TEA_TPOINTER)
#define tvisgcv(o) (itype(o) >= TEA_TSTRING)
#define tvisstr(o) (itype(o) == TEA_TSTRING)
#define tvisrange(o) (itype(o) == TEA_TRANGE)
#define tvisfunc(o) (itype(o) == TEA_TFUNC)
#define tviscfunc(o) (itype(o) == TEA_TCFUNC)
#define tvismodule(o) (itype(o) == TEA_TMODULE)
#define tvislist(o) (itype(o) == TEA_TLIST)
#define tvismap(o) (itype(o) == TEA_TMAP)
#define tvisclass(o) (itype(o) == TEA_TCLASS)
#define tvisinstance(o) (itype(o) == TEA_TINSTANCE)
#define tvismethod(o) (itype(o) == TEA_TMETHOD)
#define tvisfile(o) (itype(o) == TEA_TFILE)
#define tvisproto(o) (itype(o) == TEA_TPROTO)

/* Macros to get tagged values */
#define boolV(o) ((o)->value.b)
#define numberV(o) ((o)->value.n)
#define pointerV(o) ((o)->value.p)
#define gcV(o) ((o)->value.gc)
#define strV(o) ((GCstr*)gcV(o))
#define rangeV(o) ((GCrange*)gcV(o))
#define funcV(o) ((GCfuncT*)gcV(o))
#define cfuncV(o) ((GCfuncC*)gcV(o))
#define protoV(o) ((GCproto*)gcV(o))
#define moduleV(o) ((GCmodule*)gcV(o))
#define instanceV(o) ((GCinstance*)gcV(o))
#define methodV(o) ((GCmethod*)gcV(o))
#define classV(o) ((GCclass*)gcV(o))
#define listV(o) ((GClist*)gcV(o))
#define mapV(o) ((GCmap*)gcV(o))
#define fileV(o) ((GCfile*)gcV(o))

/* Macros to set tagged values */
#define setnullV(o) ((o)->tt = TEA_TNULL)
#define setfalseV(o) { TValue* _tv = (o); _tv->value.b = false; _tv->tt = TEA_TBOOL; }
#define settrueV(o) { TValue* _tv = (o); _tv->value.b = true; _tv->tt = TEA_TBOOL; }
#define setboolV(o, x) { TValue* _tv = (o); _tv->value.b = (x); _tv->tt = TEA_TBOOL; }
#define setnumberV(o, x) { TValue* _tv = (o); _tv->value.n = (x); _tv->tt = TEA_TNUMBER; }
#define setpointerV(o, x) { TValue* _tv = (o); _tv->value.p = (x); _tv->tt = TEA_TPOINTER; }

static TEA_AINLINE void setgcV(tea_State* T, TValue* o, GCobj* v, uint8_t tt)
{
    o->value.gc = v;
    o->tt = tt;
}

#define define_setV(name, type, tag) \
static TEA_AINLINE void name(tea_State* T, TValue* o, const type* v) \
{ \
    setgcV(T, o, (GCobj*)v, tag); \
}
define_setV(setstrV, GCstr, TEA_TSTRING)
define_setV(setrangeV, GCrange, TEA_TRANGE)
define_setV(setprotoV, GCproto, TEA_TPROTO)
define_setV(setfuncV, GCfuncT, TEA_TFUNC)
define_setV(setcfuncV, GCfuncC, TEA_TCFUNC)
define_setV(setmoduleV, GCmodule, TEA_TMODULE)
define_setV(setclassV, GCclass, TEA_TCLASS)
define_setV(setinstanceV, GCinstance, TEA_TINSTANCE)
define_setV(setmethodV, GCmethod, TEA_TMETHOD)
define_setV(setlistV, GClist, TEA_TLIST)
define_setV(setmapV, GCmap, TEA_TMAP)
define_setV(setfileV, GCfile, TEA_TFILE)
#undef define_setV

/* Copy tagged values */
static TEA_AINLINE void copyTV(tea_State* T, TValue* o1, const TValue* o2)
{
    o1->value = o2->value;
    o1->tt = o2->tt;
}

/* Names for internal and external object tags */
TEA_DATA const char* const tea_val_typenames[];
TEA_DATA const char* const tea_obj_typenames[];

#define tea_typename(o) (tea_obj_typenames[itype(o)])

#define tea_obj_new(T, type, object_type) (type*)tea_obj_alloc(T, sizeof(type), object_type)

TEA_FUNC GCobj* tea_obj_alloc(tea_State* T, size_t size, uint8_t type);

TEA_FUNC GCmodule* tea_obj_new_module(tea_State* T, GCstr* name);
TEA_FUNC GCfile* tea_obj_new_file(tea_State* T, GCstr* path, GCstr* type);
TEA_FUNC GCrange* tea_obj_new_range(tea_State* T, double start, double end, double step);
TEA_FUNC GCclass* tea_obj_new_class(tea_State* T, GCstr* name, GCclass* superclass);
TEA_FUNC GCinstance* tea_obj_new_instance(tea_State* T, GCclass* klass);
TEA_FUNC GCmethod* tea_obj_new_method(tea_State* T, TValue* receiver, TValue* method);

/* -- Object and value handling --------------------------------------- */

TEA_FUNC const void* tea_obj_pointer(TValue* v);
TEA_FUNC bool tea_val_equal(TValue* a, TValue* b);
TEA_FUNC bool tea_val_rawequal(TValue* a, TValue* b);
TEA_FUNC GCstr* tea_val_tostring(tea_State* T, TValue* value, int depth);
TEA_FUNC double tea_val_tonumber(TValue* value, bool* x);

static TEA_AINLINE bool tea_obj_isfalse(TValue* value)
{
    return  tvisnull(value) ||
            (tvisbool(value) && !boolV(value)) ||
            (tvisnumber(value) && numberV(value) == 0) ||
            (tvisstr(value) && strV(value)->chars[0] == '\0') ||
            (tvislist(value) && listV(value)->count == 0) ||
            (tvismap(value) && mapV(value)->count == 0);
}

#endif