/*
** tea_utf.h
** UTF-8 functions
*/

#ifndef _TEA_UTF_H
#define _TEA_UTF_H

#include "tea_def.h"
#include "tea_obj.h"

TEA_FUNC uint32_t tea_utf_len(GCstr* str);
TEA_FUNC int tea_utf_decode(const uint8_t* bytes, uint32_t len);
TEA_FUNC int tea_utf_encode(int value, uint8_t* bytes);

TEA_FUNC GCstr* tea_utf_codepoint_at(tea_State* T, GCstr* str, uint32_t index);
TEA_FUNC GCstr* tea_utf_from_codepoint(tea_State* T, int value);
TEA_FUNC GCstr* tea_utf_from_range(tea_State* T, GCstr* str, int start, uint32_t count, int step);
TEA_FUNC GCstr* tea_utf_reverse(tea_State* T, GCstr* str);

TEA_FUNC int tea_utf_char_offset(char* str, int index);

#endif