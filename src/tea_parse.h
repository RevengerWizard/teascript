/*
** tea_parse.h
** Teascript parser (source code -> bytecode)
*/

#ifndef _TEA_PARSE_H
#define _TEA_PARSE_H

#include "tea_lex.h"

TEA_FUNC GCproto* tea_parse(LexState* ls, bool isexpr);
TEA_FUNC void tea_parse_mark(tea_State* T, struct ParseState* parser);

#endif