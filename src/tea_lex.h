/*
** tea_lex.h
** Lexical analyzer
*/

#ifndef _TEA_LEX_H
#define _TEA_LEX_H

#include "tea.h"

#include "tea_buf.h"
#include "tea_obj.h"
#include "tea_err.h"

/* Teascript lexer tokens */
#define TKDEF(_) \
    _(AND, and) _(NOT, not) _(CLASS, class) \
    _(STATIC, static) _(OPERATOR, operator) _(ELSE, else) _(FALSE, false) \
    _(FOR, for) _(FUNCTION, function) _(CASE, case) _(SWITCH, switch) _(DEFAULT, default) \
    _(IF, if) _(NIL, nil) _(OR, or) _(IS, is) \
    _(IMPORT, import) _(FROM, from) _(AS, as) \
    _(RETURN, return) _(SUPER, super) _(SELF, self) \
    _(CONTINUE, continue) _(BREAK, break) _(IN, in) \
    _(TRUE, true) _(VAR, var) _(CONST, const) \
    _(NEW, new) _(EXPORT, export) _(DO, do) _(WHILE, while) \
    _(PLUS_EQUAL, +=) _(MINUS_EQUAL, -=) _(STAR_EQUAL, *=) _(SLASH_EQUAL, /=) \
    _(BANG_EQUAL, !=) _(EQUAL_EQUAL, ==) \
    _(GREATER_EQUAL, >=) _(LESS_EQUAL, <=) _(PERCENT_EQUAL, %=) \
    _(STAR_STAR, **) _(STAR_STAR_EQUAL, **=) \
    _(DOT_DOT, ..) _(DOT_DOT_DOT, ...) \
    _(AMPERSAND_EQUAL, &=) _(PIPE_EQUAL, |=) \
    _(CARET_EQUAL, ^=) _(ARROW, =>) \
    _(GREATER_GREATER, >>) _(LESS_LESS, <<) \
    _(GREATER_GREATER_EQUAL, >>=) _(LESS_LESS_EQUAL, <<=) \
    _(NAME, <name>) _(STRING, <string>) _(INTERPOLATION, <interpolation>) _(NUMBER, <number>) \
    _(EOF, <eof>)

enum
{
    TK_OFS = 256,
#define TKENUM(name, sym) TK_##name,
    TKDEF(TKENUM)
#undef TKENUM
    TK_RESERVED = TK_WHILE - TK_OFS
};

typedef int LexChar;  /* Lexical character*/
typedef int LexToken; /* Lexical token */

typedef struct
{
    LexToken t;
    BCLine line;
    TValue tv;
} Token;

/* Combined bytecode ins/line. Only used for bytecode generation */
typedef struct BCInsLine
{
    BCIns ins;  /* Bytecode instruction */
    BCLine line;    /* Line number for this bytecode */
} BCInsLine;

/* Info for variables. Only used for bytecode generation */
typedef struct VarInfo
{
    GCstr* name;   /* Variable name */
    bool isconst;   /* Constant variable */
    bool init;  /* Initialized variable */
} VarInfo;

/* Teascript lexer state */
typedef struct LexState
{
    tea_State* T;    /* Teascript state */
    struct FuncState* fs;   /* Current FuncState. Defined in tea_parse.c */
    SBuf sb;  /* String buffer for tokens */
    const char* p;  /* Current position in input buffer */
    const char* pe;   /* End of input buffer */
    tea_Reader reader;   /* Reader callback */
    void* rdata; /* Reader data */
    GCmodule* module; /* Current module */
    LexChar c;  /* Current character */
    LexChar sc;    /* Whether string is ' " ` */
    Token prev; /* Previous token */
    Token curr;   /* Currently used token */
    Token next; /* Lookahead token */
    BCLine linenumber;   /* Line counter */
    LexChar str_braces[4];  /* Tracked string interpolations */
    int braces[4];  /* Tracked string interpolations */
    int num_braces; /* Number of string interpolations */
    const char* mode;   /* Load bytecode (b) and/or source text (t) */
    BCInsLine* bcstack; /* Stack for bytecode instructions/line numbers */
    uint32_t sizebcstack;	/* Size of bytecode stack */
    VarInfo* vstack;  /* Variable stack */
    uint32_t vtop;  /* Top of variable stack */
    uint32_t sizevstack;	/* Size of variable stack */
    bool endmark;   /* Trust bytecode end marker, even if not at EOF */
    bool eval;  /* Evaluate expression */
} LexState;

TEA_FUNC bool tea_lex_setup(tea_State* T, LexState* ls);
TEA_FUNC void tea_lex_cleanup(tea_State* T, LexState* ls);
TEA_FUNC const char* tea_lex_token2str(LexState* ls, LexToken t);
TEA_FUNC_NORET void tea_lex_error(LexState* ls, LexToken tok, BCLine line, ErrMsg em, ...);
TEA_FUNC void tea_lex_next(LexState* ls);
TEA_FUNC void tea_lex_init(tea_State* T);

#ifdef TEA_USE_ASSERT
#define tea_assertLS(c, ...) (tea_assertT_(ls->T, (c), __VA_ARGS__))
#else
#define tea_assertLS(c, ...) ((void)0)
#endif

#endif