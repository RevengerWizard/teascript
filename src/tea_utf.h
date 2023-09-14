/*
** tea_utf.h
** UTF-8 functions for Teascript
*/

#ifndef TEA_UTF_H
#define TEA_UTF_H

#include "tea_def.h"
#include "tea_value.h"

TEA_FUNC int tea_utf_decode_bytes(uint8_t byte);
TEA_FUNC int tea_utf_encode_bytes(int value);

TEA_FUNC int tea_utf_length(TeaOString* string);
TEA_FUNC int tea_utf_decode(const uint8_t* bytes, uint32_t length);
TEA_FUNC int tea_utf_encode(int value, uint8_t* bytes);

TEA_FUNC TeaOString* tea_utf_codepoint_at(TeaState* T, TeaOString* string, uint32_t index);
TEA_FUNC TeaOString* tea_utf_from_codepoint(TeaState* T, int value);
TEA_FUNC TeaOString* tea_utf_from_range(TeaState* T, TeaOString* source, int start, uint32_t count, int step);

TEA_FUNC int tea_utf_char_offset(char* str, int index);

#endif