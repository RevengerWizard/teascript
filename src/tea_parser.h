/*
** tea_parser.h
** Teascript parser and compiler
*/

#ifndef TEA_COMPILER_H
#define TEA_COMPILER_H

#include "tea_lexer.h"
#include "tea_object.h"

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, /*  =  */
    PREC_OR,         /*  or  */
    PREC_AND,        /*  and  */
    PREC_EQUALITY,   /*  == !=  */
    PREC_IS,         /*  is  */
    PREC_COMPARISON, /*  < > <= >=  */
    PREC_BOR,        /*  |  */
    PREC_BXOR,       /*  ^  */
    PREC_BAND,       /*  &  */
    PREC_SHIFT,      /*  << >>  */
    PREC_RANGE,      /*  .. ...  */
    PREC_TERM,       /*  + -  */
    PREC_FACTOR,     /*  * /  */
    PREC_INDICES,    /*  **  */
    PREC_UNARY,      /*  not ! - ~  */
    PREC_SUBSCRIPT,  /*  []  */
    PREC_CALL,       /*  . ()  */
    PREC_PRIMARY
} TeaPrecedence;

typedef struct
{
    TeaToken name;
    int depth;
    bool is_captured;
    bool constant;
} TeaLocal;

typedef struct
{
    uint8_t index;
    bool is_local;
    bool constant;
} TeaUpvalue;

typedef struct
{
    TeaState* T;
    TeaLexer lex;
    TeaToken current;
    TeaToken previous;
    TeaOModule* module;
} TeaParser;

typedef struct TeaClassCompiler
{
    struct TeaClassCompiler* enclosing;
    bool is_static;
    bool has_superclass;
} TeaClassCompiler;

typedef struct TeaLoop
{
    struct TeaLoop* enclosing;
    int start;
    int body;
    int end;
    int scope_depth;
} TeaLoop;

typedef struct TeaCompiler
{
    TeaParser* parser;
    struct TeaCompiler* enclosing;
    TeaClassCompiler* klass;
    TeaLoop* loop;
    TeaOFunction* function;
    TeaFunctionType type;
    TeaLocal locals[UINT8_COUNT];
    int local_count;
    TeaUpvalue upvalues[UINT8_COUNT];
    int slot_count;
    int scope_depth;
} TeaCompiler;

typedef void (*TeaParseFn)(TeaCompiler* compiler, bool can_assign);

typedef struct
{
    TeaParseFn prefix;
    TeaParseFn infix;
    TeaPrecedence precedence;
} TeaParseRule;

TEA_FUNC TeaOFunction* tea_compile(TeaState* T, TeaOModule* module, const char* source);
TEA_FUNC void tea_compiler_mark_roots(TeaState* T, TeaCompiler* compiler);

#endif