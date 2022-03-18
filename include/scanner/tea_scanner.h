#ifndef TEA_SCANNER_H
#define TEA_SCANNER_H

#include "tea_predefines.h"
#include "state/tea_state.h"
#include "scanner/tea_token.h"

typedef struct TeaScanner
{
    TeaState* state;

    const char* start;
    const char* current;
    int line;
} TeaScanner;

void tea_init_scanner(TeaState* state, TeaScanner* scanner, const char* source);
void tea_back_track(TeaScanner* scanner);
TeaToken tea_scan_token(TeaScanner* scanner);

#endif