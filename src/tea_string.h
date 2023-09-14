/*
** tea_string.h
** Teascript string implementation
*/

#ifndef TEA_STRING_H
#define TEA_STRING_H

#include <string.h>

#include "tea_object.h"

#define tea_str_literal(T, s) (tea_str_copy(T, "" s, (sizeof(s)/sizeof(char))-1))
#define tea_str_new(T, s) (tea_str_copy(T, s, strlen(s)))

TEA_FUNC TeaOString* tea_str_take(TeaState* T, char* chars, int length);
TEA_FUNC TeaOString* tea_str_copy(TeaState* T, const char* chars, int length);

#endif