/*
** tea_utf.h
** UTF-8 functions for Teascript
*/

#ifndef TEA_UTF_H
#define TEA_UTF_H

#include "tea_def.h"
#include "tea_value.h"

int tea_utf_decode_bytes(uint8_t byte);
int tea_utf_encode_bytes(int value);

int tea_utf_length(TeaObjectString* string);
int tea_utf_decode(const uint8_t* bytes, uint32_t length);
int tea_utf_encode(int value, uint8_t* bytes);

TeaObjectString* tea_utf_codepoint_at(TeaState* T, TeaObjectString* string, uint32_t index);
TeaObjectString* tea_utf_from_codepoint(TeaState* T, int value);
TeaObjectString* tea_utf_from_range(TeaState* T, TeaObjectString* source, int start, uint32_t count, int step);

int tea_utf_char_offset(char* str, int index);

#endif