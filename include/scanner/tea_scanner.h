#ifndef TEA_SCANNER_H
#define TEA_SCANNER_H

#include "scanner/tea_token.h"

void tea_init_scanner(const char* source);
TeaToken tea_scan_token();

#endif