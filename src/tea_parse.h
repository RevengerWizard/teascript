/*
** tea_parse.h
** Teascript parser (source code -> bytecode)
*/

#ifndef _TEA_PARSE_H
#define _TEA_PARSE_H

#include "tea_lex.h"

TEA_FUNC GCproto* tea_parse(LexState* ls, bool eval);
TEA_FUNC GCstr* tea_parse_keepstr(LexState* ls, const char* str, size_t len);

#endif