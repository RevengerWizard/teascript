/*
** tea_state.h
** Teascript global state
*/

#ifndef _TEA_STATE_H
#define _TEA_STATE_H

#include <setjmp.h>

#include "tea.h"

#include "tea_def.h"
#include "tea_parse.h"
#include "tea_obj.h"
#include "tea_buf.h"

/* Extra stack space to handle special method calls and other extras */
#define EXTRA_STACK 5
#define BASIC_CI_SIZE 8
#define BASE_STACK_SIZE (TEA_MIN_STACK * 2)

/*
** Information about a call
*/
typedef struct
{
    GCfuncT* func;
    GCfuncC* cfunc;
    uint8_t* ip;
    int state;
    Value* base; /* Base for this function */
} CallInfo;

/*
** Bits in CallInfo state
*/
#define CIST_C    (1 << 0)  /* Call is running a C function */
#define CIST_REENTRY  (1 << 1)  /* Call is running on a new invocation of 'vm_execute' */
#define CIST_CALLING  (1 << 2)  /* Call a Tea function */
#define CIST_TEA   (1 << 3) /* Call is running a Tea function */

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

/*
** Per interpreter state
*/
typedef struct tea_State
{
    Value* stack_max;   /* Last free slot in the stack */
    Value* stack;    /* Stack base */
    Value* top;  /* First free slot in the stack */
    Value* base; /* Base of current function */
    int stack_size; 
    CallInfo* ci;    /* CallInfo for current function */
    CallInfo* ci_end;    /* Points after end of ci array */
    CallInfo* ci_base;   /* Array of CallInfos */
    int ci_size;    /* Size of array 'ci_base' */
    GCupvalue* open_upvalues; /* List of open upvalues in the stack */
    Parser* parser;
    Table modules;   /* Table of cached modules */
    Table globals;   /* Table of globals */
    Table constants;    /* Table to keep track of 'const' variables */
    Table strings;   /* Hash table for strings */
    SBuf tmpbuf;    /* Termorary string buffer */
    GCmodule* last_module;    /* Last cached module */
    GCclass* string_class;
    GCclass* list_class;
    GCclass* map_class;
    GCclass* file_class;
    GCclass* range_class;
    GCstr* constructor_string;  /* "constructor" */
    GCstr* repl_string; /* "_" */
    GCstr* memerr;  /* String message for out-of-memory situation */
    GCstr* opm_name[MM__MAX];   /* Array with special method names  */
    GCobj* objects;    /* List of all collectable objects */
    size_t bytes_allocated; /* Number of bytes currently allocated */
    size_t next_gc; /* Number of bytes to activate next GC */
    int gray_count; /* Number of grayed GC objects */
    int gray_size;
    GCobj** gray_stack; /* List of gray objects */
    struct tea_longjmp* error_jump; /* Current error recovery point */
    tea_CFunction panic; /* Function to be called in unprotected errors */
    tea_Alloc allocf;  /* Memory allocator */
    void* allocd;   /* Memory allocator data */
    int argc;
    char** argv;
    int argf;
    bool repl;
    int nccalls;    /* Number of nested C calls */
} tea_State;

#define TEA_THROW(T)    (longjmp(T->error_jump->buf, 1))
#define TEA_TRY(T, c, a)    if(setjmp((c)->buf) == 0) { a }

#define stacksave(T, p) ((char*)(p) - (char*)T->stack)
#define stackrestore(T, n)  ((Value*)((char*)T->stack + (n)))

#define cisave(T, p)        ((char*)(p) - (char*)T->ci_base)
#define cirestore(T, n)     ((CallInfo*)((char*)T->ci_base + (n)))

TEA_FUNC void tea_state_growstack(tea_State* T, int needed);
TEA_FUNC void tea_state_reallocci(tea_State* T, int new_size);
TEA_FUNC void tea_state_growci(tea_State* T);

static TEA_AINLINE void tea_state_checkstack(tea_State* T, int need)
{
    if((char*)T->stack_max - (char*)T->top <= 
       (ptrdiff_t)(need)*(ptrdiff_t)sizeof(Value))
        tea_state_growstack(T, need);
}

TEA_FUNC GCclass* tea_state_get_class(tea_State* T, Value value);
TEA_FUNC bool tea_state_isclass(tea_State* T, GCclass* klass);

#endif