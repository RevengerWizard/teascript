#ifndef TEA_PARSER_H
#define TEA_PARSER_H

#include "scanner/tea_scanner.h"
#include "vm/tea_object.h"

typedef enum
{
    PREC_NONE,
    PREC_ASSIGNMENT, // =
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < > <= >=
    PREC_BOR,        // |
    PREC_BXOR,       // ^
    PREC_BAND,       // &
    PREC_SHIFT,      // << >>
    PREC_RANGE,      // .. ...
    PREC_TERM,       // + -
    PREC_FACTOR,     // * /
    PREC_UNARY,      // ! - ~
    PREC_CALL,       // . ()
    PREC_SUBSCRIPT,  // []
    PREC_PRIMARY
} TeaPrecedence;

typedef struct
{
    TeaToken name;
    int depth;
    bool is_captured;
} TeaLocal;

typedef struct
{
    uint8_t index;
    bool is_local;
} TeaUpvalue;

typedef struct
{
    TeaToken current;
    TeaToken previous;
    bool had_error;
    bool panic_mode;
    TeaObjectModule* module;
} TeaParser;

typedef struct TeaClassCompiler
{
    struct TeaClassCompiler* enclosing;
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
    TeaState* state;

    TeaParser* parser;
    struct TeaCompiler* enclosing;
    TeaClassCompiler* klass;
    TeaLoop* loop;

    TeaObjectFunction* function;
    TeaFunctionType type;

    TeaLocal locals[UINT8_COUNT];

    int local_count;
    TeaUpvalue upvalues[UINT8_COUNT];

    int scope_depth;

    bool with_block;
    TeaToken with_file;
} TeaCompiler;

typedef void (*TeaParsePrefixFn)(TeaCompiler* compiler, bool can_assign);
typedef void (*TeaParseInfixFn)(TeaCompiler* compiler, TeaToken previous_token, bool can_assign);

typedef struct
{
    TeaParsePrefixFn prefix;
    TeaParseInfixFn infix;
    TeaPrecedence precedence;
} TeaParseRule;

TeaObjectFunction* tea_compile(TeaState* state, TeaObjectModule* module, const char* source);
void tea_mark_compiler_roots(TeaState* state);

#endif