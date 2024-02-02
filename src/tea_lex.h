/*
** tea_lex.h
** Lexical analyzer
*/

#ifndef _TEA_LEX_H
#define _TEA_LEX_H

#include "tea.h"

#include "tea_buf.h"
#include "tea_obj.h"

/* Teascript lexer tokens */
#define TKDEF(_) \
    _(PLUS_PLUS, ++) _(MINUS_MINUS, --) \
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
    _(AND, and) _(NOT, not) _(CLASS, class) _(STATIC, static) _(ELSE, else) _(FALSE, false) \
    _(FOR, for) _(FUNCTION, function) _(CASE, case) _(SWITCH, switch) _(DEFAULT, default) \
    _(IF, if) _(NULL, null) _(OR, or) _(IS, is) \
    _(IMPORT, import) _(FROM, from) _(AS, as) _(ENUM, enum) \
    _(RETURN, return) _(SUPER, super) _(THIS, this) \
    _(CONTINUE, continue) _(BREAK, break) _(IN, in) \
    _(TRUE, true) _(VAR, var) _(CONST, const) \
    _(WHILE, while) _(DO, do) \
    _(EOF, <eof>)

enum
{
    TK_OFS = 256,
#define TKENUM(name, sym) TK_##name,
    TKDEF(TKENUM)
#undef TKENUM
};

typedef struct
{
    int type;
    int line;
    Value value;
} Token;

/* Teascript lexer state */
typedef struct Lexer
{
    tea_State* T;    /* Teascript state */
    SBuf sbuf;  /* String buffer for tokens */
    const char* p;  /* Current position in input buffer */
    const char* pe;   /* End of input buffer */
    tea_Reader reader;   /* Reader callback */
    void* data; /* Reader data */
    GCmodule* module; /* Current module */
    int c;  /* Current character */
    Token prev; /* Previous token */
    Token curr;   /* Current used token */
    Token next; /* Lookahead token */
    int line;   /* Line counter */
    char string;    /* Whether string is ' or " */
    int braces[4];  /* Tracked string interpolations */
    int num_braces; /* Number of string interpolations */
    const char* mode;   /* Load bytecode (b) and/or source text (t) */
    bool endmark;   /* Trust bytecode end marker, even if not at EOF */
} Lexer;

TEA_FUNC bool tea_lex_init(tea_State* T, Lexer* lex);
TEA_FUNC void tea_lex_error(Lexer* lex, Token* token, const char* message);
TEA_FUNC void tea_lex_next(Lexer* lex);

#endif