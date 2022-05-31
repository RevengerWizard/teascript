#ifndef TEA_UTF_H
#define TEA_UTF_H

#include "tea_common.h"
#include "tea_value.h"

int tea_decode_bytes(uint8_t byte);
int tea_encode_bytes(int value);

int tea_ustring_length(TeaObjectString* string);
int tea_ustring_decode(const uint8_t* bytes, uint32_t length);
int tea_ustring_encode(int value, uint8_t* bytes);

TeaObjectString* tea_ustring_code_point_at(TeaState* state, TeaObjectString* string, uint32_t index);
TeaObjectString* tea_ustring_from_code_point(TeaState* state, int value);
TeaObjectString* tea_ustring_from_range(TeaState* state, TeaObjectString* source, int start, uint32_t count);

int lit_uchar_offset(char *str, int index);

#endif