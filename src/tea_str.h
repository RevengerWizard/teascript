/*
** tea_str.h
** String handling
*/

#ifndef _TEA_STR_H
#define _TEA_STR_H

#include <string.h>

#include "tea_def.h"
#include "tea_obj.h"

/* String interning */
TEA_FUNC GCstr* tea_str_take(tea_State* T, char* chars, int len);
TEA_FUNC GCstr* tea_str_copy(tea_State* T, const char* chars, int len);

TEA_FUNC GCstr* tea_str_format(tea_State* T, const char* format, ...);

#define tea_str_newlit(T, s) (tea_str_copy(T, "" s, (sizeof(s)/sizeof(char))-1))
#define tea_str_new(T, s) (tea_str_copy(T, s, strlen(s)))

#endif