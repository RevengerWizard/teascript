/*
** tea_state.h
** Teascript global state
*/

#ifndef TEA_STATE_H
#define TEA_STATE_H

#include <setjmp.h>

#include "tea.h"

#include "tea_def.h"
#include "tea_value.h"
#include "tea_object.h"

#define BASIC_CI_SIZE 8
#define BASE_STACK_SIZE (TEA_MIN_STACK * 2)

typedef struct
{
    TeaOClosure* closure;
    TeaONative* native;
    uint8_t* ip;
    TeaValue* base;
} TeaCallInfo;

typedef enum
{
    MT_PLUS,        /* + */
    MT_MINUS,       /* - */
    MT_MULT,        /* * */
    MT_DIV,         /* / */
    MT_MOD,         /* % */
    MT_POW,         /* ** */
    MT_BAND,        /* & */
    MT_BOR,         /* | */
    MT_BNOT,        /* ~ */
    MT_BXOR,        /* ^ */
    MT_LSHIFT,      /* << */
    MT_RSHIFT,      /* >> */
    MT_LT,          /* < */
    MT_LE,          /* <= */
    MT_GT,          /* > */
    MT_GE,          /* >= */
    MT_EQ,          /* == */
    MT_SUBSCRIPT,   /* [] */
    MT_TOSTRING,    /* tostring */
    MT_END
} TeaOpMethod;

typedef struct TeaState
{
    TeaValue* stack_last;
    TeaValue* stack;
    TeaValue* top;
    TeaValue* base;
    int stack_size;
    TeaCallInfo* ci;
    TeaCallInfo* end_ci;
    TeaCallInfo* base_ci;
    int ci_size;
    TeaOUpvalue* open_upvalues;
    TeaCompiler* compiler;
    TeaTable modules;
    TeaTable globals;
    TeaTable constants;
    TeaTable strings;
    TeaOModule* last_module;
    TeaOClass* string_class;
    TeaOClass* list_class;
    TeaOClass* map_class;
    TeaOClass* file_class;
    TeaOClass* range_class;
    TeaOString* constructor_string;
    TeaOString* repl_string;
    TeaOString* memerr;
    TeaOString* opm_name[MT_END];
    TeaObject* objects;
    size_t bytes_allocated;
    size_t next_gc;
    int gray_count;
    int gray_capacity;
    TeaObject** gray_stack;
    struct tea_longjmp* error_jump;
    TeaCFunction panic;
    TeaAlloc frealloc;
    void* ud;
    int argc;
    char** argv;
    int argf;
    bool repl;
    int nccalls;
} TeaState;

#define TEA_THROW(T)    (longjmp(T->error_jump->buf, 1))
#define TEA_TRY(T, c, a)    if(setjmp((c)->buf) == 0) { a }

TEA_FUNC TeaOClass* tea_state_get_class(TeaState* T, TeaValue value);
TEA_FUNC bool tea_state_isclass(TeaState* T, TeaOClass* klass);

#endif