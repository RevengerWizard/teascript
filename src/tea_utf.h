// tea_utf.h
// UTF-8 functions for Teascript

#ifndef TEA_UTF_H
#define TEA_UTF_H

#include "tea_common.h"
#include "tea_value.h"

int teaU_decode_bytes(uint8_t byte);
int teaU_encode_bytes(int value);

int teaU_length(TeaObjectString* string);
int teaU_decode(const uint8_t* bytes, uint32_t length);
int teaU_encode(int value, uint8_t* bytes);

TeaObjectString* teaU_code_point_at(TeaState* T, TeaObjectString* string, uint32_t index);
TeaObjectString* teaU_from_code_point(TeaState* T, int value);
TeaObjectString* teaU_from_range(TeaState* T, TeaObjectString* source, int start, uint32_t count, int step);

int teaU_char_offset(char* str, int index);

#endif