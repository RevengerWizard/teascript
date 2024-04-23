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

typedef const TValue cTValue;

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
    TEA_TMODULE,
    TEA_TCLASS,
    TEA_TINSTANCE,
    TEA_TLIST,
    TEA_TMAP,
    TEA_TFILE,
    TEA_TPROTO,
    TEA_TUPVALUE,
    TEA_TMETHOD,
};

/* GC common header */
typedef struct GCobj
{
    uint8_t gct;
    uint8_t marked;
    struct GCobj* next;
} GCobj;

/* -- Common type definitions --------------------------------------------- */

/* Types for handling bytecodes */
typedef uint8_t BCIns;

/* Resizable string buffer */
typedef struct SBuf
{
    char* w;    /* Write pointer */
    char* e;    /* End pointer */
    char* b;    /* Base pointer */
} SBuf;

/* Union to extract the bits of a double */
typedef union NumberBits
{
    uint64_t u64;   /* 64 bit pattern overlaps double */
    double n;   /* Number */
    struct
    {
        int32_t i;  /* Integer value */
        uint32_t _;
    };
    struct 
    {
        uint32_t lo;    /* Lower 32 bits of number */
        uint32_t hi;    /* Upper 32 bits of number */
    } u32;
} NumberBits;

/* -- String object -------------------------------------------------- */

typedef uint32_t StrHash;   /* String hash value */

typedef struct GCstr
{
    GCobj obj;
    uint32_t len;    /* Size of string */
    char* chars;    /* Data of string */
    StrHash hash;  /* Hash of string */
} GCstr;

#define str_data(s) ((s)->chars)

/* -- Hash table -------------------------------------------------- */

/* Hash node */
typedef struct
{
    GCstr* key;
    TValue value;
} TableEntry;

typedef struct
{
    uint32_t count;
    uint32_t size;
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
    uint32_t offset; /* Bytecode instruction */
    uint32_t line;   /* Line number for this bytecode */
} LineStart;

typedef struct
{
    GCobj obj;
    uint8_t arity;  /* Number of arguments */
    uint8_t arity_optional; /* Number of optional arguments */
    uint8_t variadic;   /* Function has variadic argument */
    uint32_t upvalue_count;  /* Number of upvalues */
    uint8_t max_slots;  /* Max stack size used by the function */
    uint8_t type;   /* Function type information */
    uint32_t bc_count;  /* Number of bytecode instructions */
    uint32_t bc_size;
    BCIns* bc;  /* Bytecode instructions */
    uint32_t k_size;
    uint32_t k_count;    /* Number of constants */
    TValue* k;  /* Constants used by the function */
    uint32_t line_count; /* Number of lines for the function definition */
    uint32_t line_size;
    LineStart* lines;   /* Map from bytecode ins. to source lines */
    GCstr* name;    /* Name of the function */
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

/* Common header of functions */
#define GCfuncHeader \
    GCobj obj; uint8_t ffid; uint8_t upvalue_count; \
    GCmodule* module

typedef enum
{
    C_FUNCTION,
    C_METHOD,
    C_PROPERTY
} CFuncType;

typedef struct
{
    GCfuncHeader;
    uint8_t type;
    tea_CFunction fn;   /* C function to be called */
    int nargs;  /* Number of arguments or -1 */
    TValue upvalues[1];   /* Array of upvalues (TValue) */
} GCfuncC;

typedef struct
{
    GCfuncHeader;
    GCproto* proto;
    GCupvalue* upvalues[1]; /* Array of _pointers_ to upvalue object */
} GCfuncT;

typedef union
{
    GCfuncC c;
    GCfuncT t;
} GCfunc;

#define FF_TEA  0
#define FF_C  1
#define isteafunc(fn)   ((fn)->c.ffid == FF_TEA)
#define iscfunc(fn)   ((fn)->c.ffid == FF_C)
#define sizeCfunc(n)    (sizeof(GCfuncC) - sizeof(TValue) + sizeof(TValue) * (n))
#define sizeTfunc(n)    (sizeof(GCfuncT) - sizeof(GCupvalue*) + sizeof(GCupvalue*) * (n))

/* -- List object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    uint32_t count;  /* Number of list items */
    uint32_t size;
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
    uint32_t count;  /* Number of map fields */
    uint32_t size;
    MapEntry* entries;
} GCmap;

/* -- Class object -------------------------------------------------- */

typedef struct GCclass
{
    GCobj obj;
    GCstr* name;
    struct GCclass* super;
    TValue constructor; /* Cached */
    Table statics;
    Table methods;
} GCclass;

/* -- Instance object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    GCclass* klass;
    Table fields;
} GCinstance;

/* -- Bound method object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    TValue receiver;
    GCfunc* method;
} GCmethod;

/* -- State objects -------------------------------------------------- */

/* Information about a call */
typedef struct
{
    GCfunc* func;
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
    _(CALL, call) \
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
    size_t total; /* Memory currently allocated */
    size_t next_gc; /* Memory threshold to activate GC */
    uint32_t gray_count; /* Number of grayed GC objects */
    uint32_t gray_size;
    GCobj** gray_stack; /* List of gray objects */
} GCState;

/* Per interpreter state */
struct tea_State
{
    TValue* stack_max;   /* Last free slot in the stack */
    TValue* stack;    /* Stack base */
    TValue* top;  /* First free slot in the stack */
    TValue* base; /* Base of current function */
    uint32_t stack_size; 
    CallInfo* ci;    /* CallInfo for current function */
    CallInfo* ci_end;    /* Points after end of ci array */
    CallInfo* ci_base;   /* CallInfo base */
    uint32_t ci_size;    /* Size of array 'ci_base' */
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
    TValue nullval; /* A null value */
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

#define nulltv(T) \
    check_exp(tvisnull(&T->nullval), &T->nullval)

#if defined(TEA_USE_ASSERT) || defined(TEA_USE_APICHECK)
TEA_FUNC_NORET void tea_assert_fail(tea_State* T, const char* file, int line, const char* func, const char* fmt, ...);
#endif

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
#define tvismodule(o) (itype(o) == TEA_TMODULE)
#define tvislist(o) (itype(o) == TEA_TLIST)
#define tvismap(o) (itype(o) == TEA_TMAP)
#define tvisclass(o) (itype(o) == TEA_TCLASS)
#define tvisinstance(o) (itype(o) == TEA_TINSTANCE)
#define tvismethod(o) (itype(o) == TEA_TMETHOD)
#define tvisfile(o) (itype(o) == TEA_TFILE)
#define tvisproto(o) (itype(o) == TEA_TPROTO)

/* Macros to get tagged values */
#define boolV(o) check_exp(tvisbool(o), (o)->value.b)
#define numberV(o) check_exp(tvisnumber(o), (o)->value.n)
#define pointerV(o) check_exp(tvispointer(o), (o)->value.p)
#define gcV(o) check_exp(tvisgcv(o), (o)->value.gc)
#define strV(o) check_exp(tvisstr(o), (GCstr*)gcV(o))
#define rangeV(o) check_exp(tvisrange(o), (GCrange*)gcV(o))
#define funcV(o) check_exp(tvisfunc(o), (GCfunc*)gcV(o))
#define protoV(o) check_exp(tvisproto(o), (GCproto*)gcV(o))
#define moduleV(o) check_exp(tvismodule(o), (GCmodule*)gcV(o))
#define instanceV(o) check_exp(tvisinstance(o), (GCinstance*)gcV(o))
#define methodV(o) check_exp(tvismethod(o), (GCmethod*)gcV(o))
#define classV(o) check_exp(tvisclass(o), (GCclass*)gcV(o))
#define listV(o) check_exp(tvislist(o), (GClist*)gcV(o))
#define mapV(o) check_exp(tvismap(o), (GCmap*)gcV(o))
#define fileV(o) check_exp(tvisfile(o), (GCfile*)gcV(o))

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
define_setV(setfuncV, GCfunc, TEA_TFUNC)
define_setV(setmoduleV, GCmodule, TEA_TMODULE)
define_setV(setclassV, GCclass, TEA_TCLASS)
define_setV(setinstanceV, GCinstance, TEA_TINSTANCE)
define_setV(setmethodV, GCmethod, TEA_TMETHOD)
define_setV(setlistV, GClist, TEA_TLIST)
define_setV(setmapV, GCmap, TEA_TMAP)
define_setV(setfileV, GCfile, TEA_TFILE)
#undef define_setV

/* Copy tagged values */
static TEA_AINLINE void copyTV(tea_State* T, TValue* o1, cTValue* o2)
{
    o1->value = o2->value;
    o1->tt = o2->tt;
}

/* Names for internal and external object tags */
TEA_DATA const char* const tea_obj_typenames[];

#define tea_typename(o) (tea_obj_typenames[itype(o)])

TEA_FUNC GCmodule* tea_obj_new_module(tea_State* T, GCstr* name);
TEA_FUNC GCfile* tea_obj_new_file(tea_State* T, GCstr* path, GCstr* type);
TEA_FUNC GCrange* tea_obj_new_range(tea_State* T, double start, double end, double step);
TEA_FUNC GCclass* tea_obj_new_class(tea_State* T, GCstr* name, GCclass* superclass);
TEA_FUNC GCinstance* tea_obj_new_instance(tea_State* T, GCclass* klass);
TEA_FUNC GCmethod* tea_obj_new_method(tea_State* T, TValue* receiver, GCfunc* method);

/* -- Object and value handling --------------------------------------- */

TEA_FUNC const void* tea_obj_pointer(cTValue* v);
TEA_FUNC bool tea_obj_equal(cTValue* a, cTValue* b);
TEA_FUNC bool tea_obj_rawequal(cTValue* a, cTValue* b);
TEA_FUNC double tea_obj_tonumber(TValue* value, bool* x);

static TEA_AINLINE bool tea_obj_isfalse(cTValue* value)
{
    return  tvisnull(value) ||
            (tvisbool(value) && !boolV(value)) ||
            (tvisnumber(value) && numberV(value) == 0) ||
            (tvisstr(value) && str_data(strV(value))[0] == '\0') ||
            (tvislist(value) && listV(value)->count == 0) ||
            (tvismap(value) && mapV(value)->count == 0);
}

#endif