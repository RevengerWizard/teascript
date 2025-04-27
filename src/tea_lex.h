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
#define TKDEF(_, __) \
    /* Keywords */ \
    _(and) _(not) _(class) \
    _(static) _(operator) _(else) _(false) \
    _(for) _(function) _(case) _(switch) _(default) \
    _(if) _(nil) _(or) _(is) \
    _(import) _(from) _(as) \
    _(return) _(super) _(self) \
    _(continue) _(break) _(in) \
    _(true) _(var) _(const) \
    _(new) _(export) _(do) _(while) \
    /* Symbols */ \
    __(noteq, !=) __(eq, ==) \
    __(ge, >=) __(le, <=) \
    __(rshift, >>) __(lshift, <<) \
    __(pluseq, +=) __(mineq, -=) \
    __(muleq, *=) __(diveq, /=) __(modeq, %=) \
    __(poweq, **=) \
    __(bandeq, &=) __(boreq, |=) __(bxoreq, ^=) \
    __(rshifteq, >>=) __(lshifteq, <<=) \
    __(pow, **) __(arrow, =>) \
    __(dotdot, ..) __(dotdotdot, ...) \
    __(name, <name>) __(string, <string>) \
    __(interpolation, <interpolation>) __(number, <number>) \
    __(eof, <eof>)

enum
{
    TK_OFS = 256,
#define TKENUM1(name) TK_##name,
#define TKENUM2(name, sym) TK_##name,
    TKDEF(TKENUM1, TKENUM2)
#undef TKENUM
    TK_RESERVED = TK_while - TK_OFS
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
    uint32_t level; /* Syntactical nesting level */
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