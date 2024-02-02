/*
** tea_utf.c
** UTF-8 functions
*/

#define tea_utf_c
#define TEA_CORE

#include "tea_def.h"

#include "tea_utf.h"
#include "tea_obj.h"
#include "tea_str.h"
#include "tea_gc.h"

static int utf_decode_bytes(uint8_t byte)
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

static int utf_encode_bytes(int value)
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

int tea_utf_len(GCstr* string)
{
	int len = 0;

	for(uint32_t i = 0; i < string->len;)
    {
		i += utf_decode_bytes(string->chars[i]);
		len++;
	}

	return len;
}

int tea_utf_decode(const uint8_t* bytes, uint32_t len)
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

	if(remaining_bytes > len - 1)
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

GCstr* tea_utf_codepoint_at(tea_State* T, GCstr* string, uint32_t index)
{
	if(index >= string->len)
    {
		return NULL;
	}

	int code_point = tea_utf_decode((uint8_t*)string->chars + index, string->len - index);

	if(code_point == -1)
    {
		char bytes[2];

		bytes[0] = string->chars[index];
		bytes[1] = '\0';

		return tea_str_copy(T, bytes, 1);
	}

	return tea_utf_from_codepoint(T, code_point);
}

GCstr* tea_utf_from_codepoint(tea_State* T, int value)
{
	int len = utf_encode_bytes(value);
	char* bytes = tea_mem_new(T, char, len + 1);
	bytes[len] = '\0';

	tea_utf_encode(value, (uint8_t*)bytes);

	return tea_str_take(T, bytes, len);
}

GCstr* tea_utf_from_range(tea_State* T, GCstr* source, int start, uint32_t count, int step)
{
	uint8_t* from = (uint8_t*)source->chars;
	int len = 0;

	for(uint32_t i = 0; i < count; i++)
    {
		len += utf_decode_bytes(from[start + i * step]);
	}

	char* bytes = tea_mem_new(T, char, len + 1);
	bytes[len] = '\0';

	uint8_t* to = (uint8_t*)bytes;

	for(uint32_t i = 0; i < count; i++)
    {
		int index = start + i * step;
		int code_point = tea_utf_decode(from + index, source->len - index);

		if(code_point != -1)
        {
			to += tea_utf_encode(code_point, to);
		}
	}

	return tea_str_take(T, bytes, len);
}

static void rev(char* str, int len)
{
    /* this assumes that str is valid UTF-8 */
    char* scanl, *scanr, *scanr2, c;

    /* first reverse the string */
    for(scanl = str, scanr = str + len; scanl < scanr;)
        c = *scanl, *scanl++ = *--scanr, *scanr = c;

    /* then scan all bytes and reverse each multibyte character */
    for(scanl = scanr = str; (c = *scanr++);)
    {
        if((c & 0x80) == 0) /* ASCII char */
            scanl = scanr;
        else if((c & 0xc0) == 0xc0)
        { /* start of multibyte */
            scanr2 = scanr;
            switch(scanr - scanl)
            {
                case 4:
                    c = *scanl, *scanl++ = *--scanr, *scanr = c; /* fallthrough */
                case 3:                                          /* fallthrough */
                case 2:
                    c = *scanl, *scanl++ = *--scanr, *scanr = c;
            }
            scanr = scanl = scanr2;
        }
    }
}

TEA_FUNC GCstr* tea_utf_reverse(tea_State* T, GCstr* string)
{
	size_t len = string->len;
	char* reversed = tea_mem_new(T, char, len + 1);
	strcpy(reversed, string->chars);
	rev(reversed, len);

    return tea_str_take(T, reversed, len);
}

int tea_utf_char_offset(char* str, int index)
{
	#define is_utf(c) (((c) & 0xc0) != 0x80)
	int offset = 0;

	while(index > 0 && str[offset])
    {
		(void)(is_utf(str[++offset]) || is_utf(str[++offset]) || is_utf(str[++offset]) || ++offset);
		index--;
	}

	return offset;
	#undef is_utf
}