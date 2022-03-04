#ifndef TEA_SCANNER_H
#define TEA_SCANNER_H

#include "scanner/tea_token.h"

typedef struct
{
    const char* start;
    const char* current;
    int line;
} TeaScanner;

void tea_init_scanner(const char* source);
TeaToken tea_scan_token();

#endif