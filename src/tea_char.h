/*
** tea_char.h
** Character types
*/

#ifndef _TEA_CHAR_H
#define _TEA_CHAR_H

#include "tea_def.h"

static TEA_INLINE bool tea_char_isident(char c)
{
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_');
}

static TEA_INLINE bool tea_char_isdigit(char c)
{
    return (c >= '0' && c <= '9');
}

static TEA_INLINE bool tea_char_isxdigit(char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'));
}

static TEA_INLINE bool tea_char_isbdigit(char c)
{
    return (c >= '0' && c <= '1');
}

static TEA_INLINE bool tea_char_iscdigit(char c)
{
    return (c >= '0' && c <= '7');
}

#endif