#ifndef TEA_SCANNER_H
#define TEA_SCANNER_H

#include "tea_predefines.h"
#include "tea_state.h"
#include "tea_token.h"

typedef struct TeaScanner
{
    TeaState* state;

    const char* start;
    const char* current;
    int line;

    char string;

    int braces[4];
    int num_braces;

    bool raw;
} TeaScanner;

void tea_init_scanner(TeaState* state, TeaScanner* scanner, const char* source);
void tea_back_track(TeaScanner* scanner);
TeaToken tea_scan_token(TeaScanner* scanner);

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

#endif