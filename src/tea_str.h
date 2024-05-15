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
TEA_FUNC GCstr* tea_str_new(tea_State* T, const char* chars, size_t len);
TEA_FUNC void tea_str_free(tea_State* T, GCstr* str);

#define tea_str_newlit(T, s) (tea_str_new(T, "" s, (sizeof(s)/sizeof(char))-1))
#define tea_str_newlen(T, s) (tea_str_new(T, s, strlen(s)))
#define tea_str_size(len) (sizeof(GCstr) + (len) + 1)

#endif