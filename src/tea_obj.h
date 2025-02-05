/*
** tea_obj.h
** Teascript VM tags, values and objects
*/

#ifndef _TEA_OBJ_H
#define _TEA_OBJ_H

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
    TEA_TNIL,
    TEA_TBOOL,
    TEA_TNUM,
    TEA_TPOINTER,
    TEA_TSTR,
    TEA_TRANGE,
    TEA_TFUNC,
    TEA_TMODULE,
    TEA_TCLASS,
    TEA_TINSTANCE,
    TEA_TLIST,
    TEA_TMAP,
    TEA_TUDATA,
    TEA_TPROTO,
    TEA_TUPVAL,
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
typedef uint8_t BCIns;  /* Bytecode instruction */
typedef uint32_t BCPos; /* Bytecode position */
typedef int32_t BCLine; /* Bytecode line number */

/* Resizable string buffer. Need this here, details in tea_buf.h */
#define SBufHeader  char* w, *e, *b; uint8_t flag
typedef struct SBuf
{
    SBufHeader;
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

/* String object header. String payload follows */
typedef struct GCstr
{
    GCobj obj;
    uint8_t reserved;   /* Used by lexer for fast lookup of reserved words */
    StrHash hash;  /* Hash of string */
    uint32_t len;    /* Size of string */
} GCstr;

#define str_data(s) ((const char*)((s) + 1))
#define str_datawr(s) ((char*)((s) + 1))
#define strVdata(o) str_data(strV(o))

/* -- Hash table -------------------------------------------------- */

/* Hash node */
typedef struct
{
    GCstr* key;
    TValue val;
} TabEntry;

typedef struct
{
    uint32_t count;
    uint32_t size;
    TabEntry* entries;
} Tab;

/* -- Range object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    double start;
    double end;
    double step;
} GCrange;

/* -- Module object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    GCstr* name;    /* Canonical module name */
    GCstr* path;    /* Absolute module path */
    TValue* vars;   /* Array of variables */
    GCstr** varnames;  /* Array of variable names */
    uint16_t size;  /* Number of variables */
    Tab exports;  /* Table of exported variables */
} GCmodule;

/* -- Prototype object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    uint8_t numparams;  /* Number of parameters */
    uint8_t numopts; /* Number of optional parameters */
    uint8_t sizeuv;  /* Number of upvalues */
    uint8_t max_slots;  /* Max stack size used by the function */
    uint8_t flags;   /* Miscellaneous flags */
    uint32_t sizept;    /* Total size including colocated arrays */
    uint32_t sizebc;  /* Number of bytecode instructions */
    uint32_t sizek;    /* Number of constants */
    TValue* k;  /* Constants used by the function */
    uint16_t* uv;   /* Upvalue list */
    /* ------ The following fields are for debugging/tracebacks only ------ */
    BCLine firstline;   /* First line of the code this function was defined in */
    BCLine numline; /* Number of lines for the function definition */
    void* lineinfo; /* Compressed map from bytecode ins. to source line */
    GCstr* name;    /* Name of the function */
} GCproto;

/* Flags for prototype */
#define PROTO_CHILD 0x1 /* Has child prototypes */
#define PROTO_VARARG 0x02   /* Variadic function */

#define PROTO_UV_LOCAL 0x0100   /* Upvalue for local slot */

#define proto_kgc(pt, i) (&((pt)->k[(i)]))
#define proto_bc(pt) ((BCIns*)((char*)(pt) + sizeof(GCproto)))
#define proto_bcpos(pt, pc) ((BCPos)((pc) - proto_bc(pt)))

/* -- Upvalue object -------------------------------------------------- */

typedef struct GCupval
{
    GCobj obj;
    TValue* location;
    TValue closed;
    struct GCupval* next;
} GCupval;

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
    int nargs;  /* Number of arguments */
    int nopts; /* Number of optional arguments */
    TValue upvalues[1];   /* Array of upvalues (TValue) */
} GCfuncC;

typedef struct
{
    GCfuncHeader;
    GCproto* pt;
    GCupval* upvalues[1]; /* Array of _pointers_ to upvalue object */
} GCfuncT;

typedef union
{
    GCfuncC c;
    GCfuncT t;
} GCfunc;

#define FF_TEA  0
#define FF_C  1
#define isteafunc(fn)   ((fn)->c.ffid == FF_TEA)
#define iscfunc(fn)     ((fn)->c.ffid == FF_C)
#define funcproto(fn)   (funcV(fn)->t.pt)
#define sizeCfunc(n)    (sizeof(GCfuncC) - sizeof(TValue) + sizeof(TValue) * (n))
#define sizeTfunc(n)    (sizeof(GCfuncT) - sizeof(GCupval*) + sizeof(GCupval*) * (n))

/* -- List object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    uint32_t len;  /* Number of list items */
    uint32_t size;
    TValue* items;  /* Array of list values */
} GClist;

#define list_slot(l, i) (&((l)->items)[(i)])

/* -- Map object -------------------------------------------------- */

/* Map node */
typedef struct
{
    TValue key;
    TValue val;
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
    struct GCclass* super;  /* Inherited class or NULL */
    TValue init; /* Cached */
    Tab methods;
} GCclass;

/* -- Instance object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    GCclass* klass; /* Instance class */
    Tab attrs;    /* Instance attributes */
} GCinstance;

/* -- Userdata object ----------------------------------------------------- */

/* Userdata object. Payload follows */
typedef struct GCudata
{
    GCobj obj;
    GCclass* klass;
    Tab attrs;
    uint8_t udtype; /* Userdata type */
    uint8_t nuvals;    /* Number of uservalues */
    uint32_t len;
    tea_Finalizer fd;
} GCudata;

/* Userdata types */
enum
{
    UDTYPE_USERDATA,    /* Regular userdata */
    UDTYPE_IOFILE,  /* io module FILE */
    UDTYPE_BUFFER,  /* String buffer */
    UDTYPE__MAX
};

#define ud_uvalues(u) ((TValue*)((char*)(u) + sizeof(GCudata)))
#define ud_data(u) ((void*)(ud_uvalues(u) + (u)->nuvals))

/* -- Bound method object -------------------------------------------------- */

typedef struct
{
    GCobj obj;
    TValue receiver;    /* "this" object */
    GCfunc* func;   /* Function to bound */
} GCmethod;

/* -- State objects -------------------------------------------------- */

/* Information about a call */
typedef struct
{
    GCfunc* func;
    uint8_t* ip;
    uint8_t state;
    TValue* base; /* Base for this function */
} CallInfo;

/* Flags for CallInfo state */
#define CIST_C    (1 << 0)  /* Call is running a C function */
#define CIST_REENTRY  (1 << 1)  /* Call is running on a new invocation of 'vm_execute' */
#define CIST_CALLING  (1 << 2)  /* Call a Teascript function */
#define CIST_TEA   (1 << 3) /* Call is running a Teascript function */

#define iscci(T) \
    (((T)->ci->func != NULL) && \
    iscfunc((T)->ci->func) && \
    (T)->ci->func->c.type == C_FUNCTION)

/* Special methods */
#define MMDEF(_) \
    _(PLUS, +) _(MINUS, -) _(MULT, *) _(DIV, /) \
    _(MOD, %) _(POW, **) _(BAND, &) _(BOR, |) _(BNOT, ~) \
    _(BXOR, ^) _(LSHIFT, <<) _(RSHIFT, >>) \
    _(LT, <) _(LE, <=) _(GT, >) _(GE, >=) _(EQ, ==) \
    _(GETINDEX, []) _(SETINDEX, []=) \
    _(GETATTR, getattr) _(SETATTR, setattr) \
    _(TOSTRING, tostring) _(CALL, call) \
    _(ITER, iterate) _(NEXT, iteratorvalue) \
    _(CONTAINS, contains) _(GC, gc) \
    _(NEW, new)

typedef enum
{
#define MMENUM(name, _) MM_##name,
MMDEF(MMENUM)
#undef MMENUM
    MM__MAX
} MMS;

#define mmname_str(T, mm) ((T)->opm_name[(mm)])

/* Garbage collector state */
typedef struct GCState
{
    GCobj* root;    /* List of (almost) all collectable objects */
    size_t total; /* Memory currently allocated */
    size_t next_gc; /* Memory threshold to activate GC */
    uint32_t gray_count; /* Number of grayed GC objects */
    uint32_t gray_size;
    GCobj** gray_stack; /* List of gray objects */
    GCobj* rootud;  /* (Separated) list of all userdata */
    GCobj* mmudata; /* List of userdata to be GC */
} GCState;

/* String interning state */
typedef struct StrInternState
{
    GCobj** hash;   /* String hash table anchors */
    uint32_t size;  /* Size of hash table */
    uint32_t num;   /* Number of strings in hash table */
} StrInternState;

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
    GCupval* open_upvalues; /* List of open upvalues in the stack */
    struct tea_longjmp* error_jump; /* Current error recovery point */
    uint16_t nccalls;    /* Number of nested C calls */
    /* ------ The following fields are global to the state ------ */
    GCState gc; /* Garbage collector */
    StrInternState str;   /* String interning */
    Tab modules;   /* Table of cached modules */
    Tab globals;   /* Table of globals */
    SBuf tmpbuf;    /* Termorary string buffer */
    SBuf strbuf;    /* Termorary string conversion buffer */
    TValue tmptv;   /* Temporary TValue */
    TValue nilval; /* A nil value */
    TValue registrytv;  /* Anchor for registry */
    GCmodule* last_module;    /* Last cached module */
    GCclass* number_class;
    GCclass* bool_class;
    GCclass* func_class;
    GCclass* string_class;
    GCclass* list_class;
    GCclass* map_class;
    GCclass* range_class;
    GCclass* object_class;
    GCstr strempty; /* Empty string */
    uint8_t strempty0;  /* Zero terminator for empty string */
    GCstr* opm_name[MM__MAX];   /* Array with special method names  */
    tea_CFunction panic; /* Function to be called in unprotected errors */
    tea_Alloc allocf;  /* Memory allocator */
    void* allocd;   /* Memory allocator data */
    int argc;
    char** argv;
    int argf;
    struct tea_handle* handle;  /* Dynamic library handles still open */
};

#define curr_func(T) (T->ci->func)

#define niltv(T) (&(T)->nilval)
#define registry(T) (&(T)->registrytv)

#if defined(TEA_USE_ASSERT) || defined(TEA_USE_APICHECK)
TEA_FUNC_NORET void tea_assert_fail(tea_State* T, const char* file, int line, const char* func, const char* fmt, ...);
#endif

/* The klass and attrs fields MUST be at the same offset */
TEA_STATIC_ASSERT(offsetof(GCinstance, klass) == offsetof(GCudata, klass));
TEA_STATIC_ASSERT(offsetof(GCinstance, attrs) == offsetof(GCudata, attrs));

/* Macros to convert a GCobj pointer to a specific value */
#define gco2str(o) ((GCstr*)(o))
#define gco2range(o) ((GCrange*)(o))
#define gco2func(o) ((GCfunc*)(o))
#define gco2proto(o) ((GCproto*)(o))
#define gco2uv(o) ((GCupval*)(o))
#define gco2module(o) ((GCmodule*)(o))
#define gco2class(o) ((GCclass*)(o))
#define gco2instance(o) ((GCinstance*)(o))
#define gco2method(o) ((GCmethod*)(o))
#define gco2list(o) ((GClist*)(o))
#define gco2map(o) ((GCmap*)(o))
#define gco2udata(o) ((GCudata*)(o))

/* Macros to convert any collectable object into a GCobj pointer */
#define obj2gco(v) ((GCobj*)(v))

/* -- TValue getters/setters -------------------------------------------------- */

/* Macros to test types */
#define itype(o) ((o)->tt)
#define tvisnil(o) (itype(o) == TEA_TNIL)
#define tvisbool(o) (itype(o) == TEA_TBOOL)
#define tvisfalse(o) ((itype(o) == TEA_TBOOL) && (!boolV(o)))
#define tvistrue(o) ((itype(o) == TEA_TBOOL) && (boolV(o)))
#define tvisnum(o) (itype(o) == TEA_TNUM)
#define tvispointer(o) (itype(o) == TEA_TPOINTER)
#define tvisgcv(o) (itype(o) >= TEA_TSTR)
#define tvisstr(o) (itype(o) == TEA_TSTR)
#define tvisrange(o) (itype(o) == TEA_TRANGE)
#define tvisfunc(o) (itype(o) == TEA_TFUNC)
#define tvismodule(o) (itype(o) == TEA_TMODULE)
#define tvislist(o) (itype(o) == TEA_TLIST)
#define tvismap(o) (itype(o) == TEA_TMAP)
#define tvisclass(o) (itype(o) == TEA_TCLASS)
#define tvisinstance(o) (itype(o) == TEA_TINSTANCE)
#define tvismethod(o) (itype(o) == TEA_TMETHOD)
#define tvisproto(o) (itype(o) == TEA_TPROTO)
#define tvisudata(o) (itype(o) == TEA_TUDATA)

/* Macros to get tagged values */
#define boolV(o) ((o)->value.b)
#define numV(o) ((o)->value.n)
#define pointerV(o) ((o)->value.p)
#define gcV(o) ((o)->value.gc)
#define strV(o) ((GCstr*)gcV(o))
#define rangeV(o) ((GCrange*)gcV(o))
#define funcV(o) ((GCfunc*)gcV(o))
#define protoV(o) ((GCproto*)gcV(o))
#define moduleV(o) ((GCmodule*)gcV(o))
#define instanceV(o) ((GCinstance*)gcV(o))
#define methodV(o) ((GCmethod*)gcV(o))
#define classV(o) ((GCclass*)gcV(o))
#define listV(o) ((GClist*)gcV(o))
#define mapV(o) ((GCmap*)gcV(o))
#define udataV(o) ((GCudata*)gcV(o))

/* Macros to set tagged values */
#define setnilV(o) ((o)->tt = TEA_TNIL)
#define setfalseV(o) { TValue* _tv = (o); _tv->value.b = false; _tv->tt = TEA_TBOOL; }
#define settrueV(o) { TValue* _tv = (o); _tv->value.b = true; _tv->tt = TEA_TBOOL; }
#define setboolV(o, x) { TValue* _tv = (o); _tv->value.b = (x); _tv->tt = TEA_TBOOL; }
#define setnumV(o, x) { TValue* _tv = (o); _tv->value.n = (x); _tv->tt = TEA_TNUM; }
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
define_setV(setstrV, GCstr, TEA_TSTR)
define_setV(setrangeV, GCrange, TEA_TRANGE)
define_setV(setprotoV, GCproto, TEA_TPROTO)
define_setV(setfuncV, GCfunc, TEA_TFUNC)
define_setV(setmoduleV, GCmodule, TEA_TMODULE)
define_setV(setclassV, GCclass, TEA_TCLASS)
define_setV(setinstanceV, GCinstance, TEA_TINSTANCE)
define_setV(setmethodV, GCmethod, TEA_TMETHOD)
define_setV(setlistV, GClist, TEA_TLIST)
define_setV(setmapV, GCmap, TEA_TMAP)
define_setV(setudataV, GCudata, TEA_TUDATA)
#undef define_setV

static TEA_AINLINE void setintV(TValue* o, int32_t i)
{
    setnumV(o, (double)i);
}

static TEA_AINLINE void setint64V(TValue* o, int64_t i)
{
    setnumV(o, (double)i);
}

/* Copy tagged values */
static TEA_AINLINE void copyTV(tea_State* T, TValue* o1, cTValue* o2)
{
    o1->value = o2->value;
    o1->tt = o2->tt;
}

/* Names for internal and external object tags */
TEA_DATA const char* const tea_obj_typenames[];

#define tea_typename(o) (tea_obj_typenames[itype(o)])

TEA_FUNC GCmodule* tea_module_new(tea_State* T, GCstr* name);
TEA_FUNC GCmodule* tea_submodule_new(tea_State* T, GCstr* name);
TEA_FUNC GCrange* tea_range_new(tea_State* T, double start, double end, double step);
TEA_FUNC GCclass* tea_class_new(tea_State* T, GCstr* name);
TEA_FUNC GCinstance* tea_instance_new(tea_State* T, GCclass* klass);
TEA_FUNC GCmethod* tea_method_new(tea_State* T, TValue* receiver, GCfunc* func);

TEA_FUNC void TEA_FASTCALL tea_module_free(tea_State* T, GCmodule* module);
TEA_FUNC void TEA_FASTCALL tea_range_free(tea_State* T, GCrange* range);
TEA_FUNC void TEA_FASTCALL tea_class_free(tea_State* T, GCclass* klass);
TEA_FUNC void TEA_FASTCALL tea_instance_free(tea_State* T, GCinstance* instance);
TEA_FUNC void TEA_FASTCALL tea_method_free(tea_State* T, GCmethod* method);

/* -- Object and value handling --------------------------------------- */

TEA_FUNC const void* tea_obj_pointer(cTValue* v);
TEA_FUNC bool tea_obj_equal(cTValue* a, cTValue* b);
TEA_FUNC bool tea_obj_rawequal(cTValue* a, cTValue* b);
TEA_FUNC double tea_obj_tonum(TValue* value, bool* x);

static TEA_AINLINE bool tea_obj_isfalse(cTValue* value)
{
    return  tvisnil(value) ||
            (tvisbool(value) && !boolV(value)) ||
            (tvisnum(value) && numV(value) == 0) ||
            (tvisstr(value) && str_data(strV(value))[0] == '\0') ||
            (tvislist(value) && listV(value)->len == 0) ||
            (tvismap(value) && mapV(value)->count == 0);
}

#endif