/*
** tea_char.h
** Character types
*/

#ifndef _TEA_CHAR_H
#define _TEA_CHAR_H

#include "tea_def.h"

#define TEA_CHAR_CNTRL  0x01
#define TEA_CHAR_SPACE  0x02
#define TEA_CHAR_PUNCT  0x04
#define TEA_CHAR_DIGIT  0x08
#define TEA_CHAR_XDIGIT 0x10
#define TEA_CHAR_UPPER  0x20
#define TEA_CHAR_LOWER  0x40
#define TEA_CHAR_IDENT  0x80
#define TEA_CHAR_ALPHA  (TEA_CHAR_LOWER | TEA_CHAR_UPPER)
#define TEA_CHAR_ALNUM  (TEA_CHAR_ALPHA | TEA_CHAR_DIGIT)
#define TEA_CHAR_GRAPH  (TEA_CHAR_ALNUM | TEA_CHAR_PUNCT)

/* Only pass -1 or 0..255 to these macros. Never pass a signed char! */
#define tea_char_isa(c, t)  ((tea_char_bits+1)[(c)] & t)
#define tea_char_iscntrl(c) tea_char_isa((c), TEA_CHAR_CNTRL)
#define tea_char_isspace(c) tea_char_isa((c), TEA_CHAR_SPACE)
#define tea_char_ispunct(c) tea_char_isa((c), TEA_CHAR_PUNCT)
#define tea_char_isdigit(c) tea_char_isa((c), TEA_CHAR_DIGIT)
#define tea_char_isxdigit(c) tea_char_isa((c), TEA_CHAR_XDIGIT)
#define tea_char_isupper(c) tea_char_isa((c), TEA_CHAR_UPPER)
#define tea_char_islower(c) tea_char_isa((c), TEA_CHAR_LOWER)
#define tea_char_isident(c) tea_char_isa((c), TEA_CHAR_IDENT)
#define tea_char_isalpha(c) tea_char_isa((c), TEA_CHAR_ALPHA)
#define tea_char_isalnum(c) tea_char_isa((c), TEA_CHAR_ALNUM)
#define tea_char_isgraph(c) tea_char_isa((c), TEA_CHAR_GRAPH)

#define tea_char_toupper(c) ((c) - (tea_char_islower(c) >> 1))
#define tea_char_tolower(c) ((c) + tea_char_isupper(c))

TEA_DATA const uint8_t tea_char_bits[257];

#endif