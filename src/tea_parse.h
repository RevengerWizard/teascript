/*
** tea_parse.h
** Teascript parser (source code -> bytecode)
*/

#ifndef _TEA_PARSE_H
#define _TEA_PARSE_H

#include "tea_lex.h"

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
} Precedence;

typedef struct
{
    Token name;
    int depth;
    bool is_captured;
    bool constant;
} Local;

typedef struct
{
    uint8_t index;
    bool is_local;
    bool constant;
} Upvalue;

typedef struct ClassParser
{
    struct ClassParser* enclosing;
    bool is_static;
    bool has_superclass;
} ClassParser;

typedef struct Loop
{
    struct Loop* enclosing;
    int start;
    int body;
    int end;
    int scope_depth;
} Loop;

typedef struct Parser
{
    Lexer* lex; /* Lexer state */
    struct Parser* enclosing;   /* Enclosing parser */
    ClassParser* klass; /* Current class parser */
    Loop* loop; /* Current loop context */
    GCproto* proto; /* Current prototype function */
    GCstr* name;    /* Name of prototype function */
    ProtoType type;
    Local locals[UINT8_COUNT];  /* Current scoped locals */
    int local_count;    /* Number of local variables in scope */
    Upvalue upvalues[UINT8_COUNT];  /* Saved upvalues */
    int slot_count; /* Stack max size */
    int scope_depth;    /* Current scope depth */
} Parser;

typedef void (*ParseFn)(Parser* compiler, bool assign);

typedef struct
{
    ParseFn prefix;
    ParseFn infix;
    Precedence prec;
} ParseRule;

TEA_FUNC GCproto* tea_parse(Lexer* lexer);
TEA_FUNC void tea_parse_mark(tea_State* T, Parser* compiler);

#endif