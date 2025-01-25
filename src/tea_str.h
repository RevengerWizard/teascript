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
TEA_FUNC void tea_str_resize(tea_State* T, uint32_t newsize);
TEA_FUNC GCstr* tea_str_new(tea_State* T, const char* chars, size_t lenx);
TEA_FUNC void TEA_FASTCALL tea_str_free(tea_State* T, GCstr* str);
TEA_FUNC void TEA_FASTCALL tea_str_init(tea_State* T);

#define tea_str_freetab(T) \
    (tea_mem_freevec(T, GCobj*, T->str.hash, T->str.size))
#define tea_str_newlit(T, s) (tea_str_new(T, "" s, (sizeof(s)/sizeof(char))-1))
#define tea_str_newlen(T, s) (tea_str_new(T, s, strlen(s)))
#define tea_str_size(len) (sizeof(GCstr) + (len) + 1)

#endif