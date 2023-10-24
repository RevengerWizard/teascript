/*
** tea_lexer.h
** Teascript lexer
*/

#ifndef TEA_LEXER_H
#define TEA_LEXER_H

#include "tea_state.h"
#include "tea_token.h"

typedef struct TeaLexer
{
    TeaState* T;
    TeaOModule* module;
    const char* start;
    const char* curr;
    TeaToken current;
    TeaToken previous;
    int line;
    char string;
    int braces[4];
    int num_braces;
    bool raw;
} TeaLexer;

TEA_FUNC void tea_lex_init(TeaState* T, TeaLexer* lex, const char* source);
TEA_FUNC void tea_lex_backtrack(TeaLexer* lex);
TEA_FUNC void tea_lex_error(TeaLexer* lex, TeaToken* token, const char* message);
TEA_FUNC TeaToken tea_lex_token(TeaLexer* lex);

static inline bool is_alpha(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

static inline bool is_digit(char c)
{
    return (c >= '0' && c <= '9');
}

static inline bool is_hex_digit(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'));
}

static inline bool is_binary_digit(char c)
{
    return (c >= '0' && c <= '1');
}

static inline bool is_octal_digit(char c)
{
    return (c >= '0' && c <= '7');
}

#endif