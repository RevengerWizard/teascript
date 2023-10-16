/*
** tea_utf.c
** UTF-8 functions for Teascript
*/

#define tea_utf_c
#define TEA_CORE

#include "tea_utf.h"
#include "tea_object.h"
#include "tea_string.h"

int tea_utf_decode_bytes(uint8_t byte)
{
    if((byte & 0xc0) == 0x80)
    {
		return 0;
	}

	if((byte & 0xf8) == 0xf0)
    {
		return 4;
	}

	if((byte & 0xf0) == 0xe0)
    {
		return 3;
	}

	if((byte & 0xe0) == 0xc0)
    {
		return 2;
	}

	return 1;
}

int tea_utf_encode_bytes(int value)
{
	if(value <= 0x7f)
    {
		return 1;
	}

	if(value <= 0x7ff)
    {
		return 2;
	}

	if(value <= 0xffff)
    {
		return 3;
	}

	if(value <= 0x10ffff)
    {
		return 4;
	}

	return 0;
}

int tea_utf_length(TeaOString* string)
{
	int length = 0;

	for(uint32_t i = 0; i < string->length;)
    {
		i += tea_utf_decode_bytes(string->chars[i]);
		length++;
	}

	return length;
}

int tea_utf_decode(const uint8_t* bytes, uint32_t length)
{
	if(*bytes <= 0x7f)
    {
		return *bytes;
	}

	int value;
	uint32_t remaining_bytes;

	if((*bytes & 0xe0) == 0xc0)
    {
		value = *bytes & 0x1f;
		remaining_bytes = 1;
	}
    else if((*bytes & 0xf0) == 0xe0)
    {
		value = *bytes & 0x0f;
		remaining_bytes = 2;
	}
    else if((*bytes & 0xf8) == 0xf0)
    {
		value = *bytes & 0x07;
		remaining_bytes = 3;
	}
    else
    {
		return -1;
	}

	if(remaining_bytes > length - 1)
    {
		return -1;
	}

	while(remaining_bytes > 0)
    {
		bytes++;
		remaining_bytes--;

		if((*bytes & 0xc0) != 0x80)
        {
			return -1;
		}

		value = value << 6 | (*bytes & 0x3f);
	}

	return value;
}

int tea_utf_encode(int value, uint8_t* bytes)
{
	if(value <= 0x7f)
    {
		*bytes = value & 0x7f;
		return 1;
	}
    else if(value <= 0x7ff)
    {
		*bytes = 0xc0 | ((value & 0x7c0) >> 6);
		bytes++;
		*bytes = 0x80 | (value & 0x3f);
		return 2;
	}
    else if(value <= 0xffff)
    {
		*bytes = 0xe0 | ((value & 0xf000) >> 12);
		bytes++;
		*bytes = 0x80 | ((value & 0xfc0) >> 6);
		bytes++;
		*bytes = 0x80 | (value & 0x3f);

		return 3;
	}
    else if(value <= 0x10ffff)
    {
		*bytes = 0xf0 | ((value & 0x1c0000) >> 18);
		bytes++;
		*bytes = 0x80 | ((value & 0x3f000) >> 12);
		bytes++;
		*bytes = 0x80 | ((value & 0xfc0) >> 6);
		bytes++;
		*bytes = 0x80 | (value & 0x3f);

		return 4;
	}

	return 0;
}

TeaOString* tea_utf_codepoint_at(TeaState* T, TeaOString* string, uint32_t index)
{
	if(index >= string->length)
    {
		return NULL;
	}

	int code_point = tea_utf_decode((uint8_t*)string->chars + index, string->length - index);

	if(code_point == -1)
    {
		char bytes[2];

		bytes[0] = string->chars[index];
		bytes[1] = '\0';

		return tea_str_copy(T, bytes, 1);
	}

	return tea_utf_from_codepoint(T, code_point);
}

TeaOString* tea_utf_from_codepoint(TeaState* T, int value)
{
	int length = tea_utf_encode_bytes(value);
	char* bytes = TEA_ALLOCATE(T, char, length + 1);
	bytes[length] = '\0';

	tea_utf_encode(value, (uint8_t*)bytes);

	return tea_str_take(T, bytes, length);
}

TeaOString* tea_utf_from_range(TeaState* T, TeaOString* source, int start, uint32_t count, int step)
{
	uint8_t* from = (uint8_t*)source->chars;
	int length = 0;

	for(uint32_t i = 0; i < count; i++)
    {
		length += tea_utf_decode_bytes(from[start + i * step]);
	}

	char* bytes = TEA_ALLOCATE(T, char, length + 1);
	bytes[length] = '\0';

	uint8_t* to = (uint8_t*)bytes;

	for(uint32_t i = 0; i < count; i++)
    {
		int index = start + i * step;
		int code_point = tea_utf_decode(from + index, source->length - index);

		if(code_point != -1)
        {
			to += tea_utf_encode(code_point, to);
		}
	}

	return tea_str_take(T, bytes, length);
}

int tea_utf_char_offset(char* str, int index)
{
	#define is_utf(c) (((c) & 0xC0) != 0x80)
	int offset = 0;

	while(index > 0 && str[offset])
    {
		(void)(is_utf(str[++offset]) || is_utf(str[++offset]) || is_utf(str[++offset]) || ++offset);
		index--;
	}

	return offset;
	#undef is_utf
}