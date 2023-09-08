/*
** tea_string.h
** Teascript string implementation
*/

#ifndef TEA_STRING_H
#define TEA_STRING_H

#include <string.h>

#include "tea_object.h"

#define tea_string_literal(T, s) (tea_string_copy(T, "" s, (sizeof(s)/sizeof(char))-1))
#define tea_string_new(T, s) (tea_string_copy(T, s, strlen(s)))

TEA_FUNC TeaObjectString* tea_string_take(TeaState* T, char* chars, int length);
TEA_FUNC TeaObjectString* tea_string_copy(TeaState* T, const char* chars, int length);

#endif