/*
** lib_utf8.c
** Teascript UTF-8 module
*/

#include <math.h>

#define lib_utf8_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_obj.h"
#include "tea_str.h"
#include "tea_buf.h"
#include "tea_lib.h"

/* -- UTF-8 encoding/decoding -------------------------------------------------- */

static int utf8_decode_bytes(uint8_t byte)
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

static int utf8_encode_bytes(int value)
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

int utf8_decode(const uint8_t* bytes, uint32_t len)
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

int utf8_encode(int value, uint8_t* bytes)
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

/* -- UTF-8 transformations -------------------------------------------------- */

#define is_utf(c) (((c) & 0xc0) != 0x80)

int utf8_char_offset(char* str, int idx)
{
    int ofs = 0;
    while(idx > 0 && str[ofs])
    {
        (void)(is_utf(str[++ofs]) || is_utf(str[++ofs]) || is_utf(str[++ofs]) || ++ofs);
        idx--;
    }
    return ofs;
}

#undef is_utf

GCstr* utf8_from_codepoint(tea_State* T, int value)
{
    int len = utf8_encode_bytes(value);
    char bytes[5];  /* Maximum bytes for UTF-8 code point (4) + \0 */
    utf8_encode(value, (uint8_t*)bytes);
    return tea_str_new(T, bytes, len);
}

GCstr* utf8_codepoint_at(tea_State* T, GCstr* str, uint32_t idx)
{
    if(idx >= str->len)
    {
        return NULL;
    }

    int code_point = utf8_decode((uint8_t*)str_data(str) + idx, str->len - idx);
    if(code_point == -1)
    {
        char bytes[2];
        bytes[0] = str_data(str)[idx];
        bytes[1] = '\0';
        return tea_str_new(T, bytes, 1);
    }
    return utf8_from_codepoint(T, code_point);
}

/* ------------------------------------------------------------------------ */

static void utf8_len(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    uint32_t len = 0;
    for(uint32_t i = 0; i < str->len;)
    {
        i += utf8_decode_bytes(str_data(str)[i]);
        len++;
    }
    tea_push_number(T, len);
}

static void utf8_char(tea_State* T)
{
    uint32_t n = (uint32_t)tea_lib_checkint(T, 0);
    setstrV(T, T->top++, utf8_from_codepoint(T, n));
}

static void utf8_ord(tea_State* T)
{
    GCstr* c = tea_lib_checkstr(T, 0);
    tea_push_number(T, utf8_decode((uint8_t*)str_data(c), c->len));
}

static void utf8_reverse(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    size_t len = str->len;
    char* rev = tea_buf_tmp(T, len);
    memcpy(rev, str_data(str), len);

    /* this assumes that the string is valid UTF-8 */
    char* scanl, *scanr, *scanr2, c;

    /* first reverse the string */
    for(scanl = rev, scanr = rev + len; scanl < scanr;)
        c = *scanl, *scanl++ = *--scanr, *scanr = c;

    /* then scan all bytes and reverse each multibyte character */
    for(scanl = scanr = rev; (c = *scanr++);)
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
    str = tea_str_new(T, rev, len);
    setstrV(T, T->top++, str);
}

static void utf8_iternext(tea_State* T)
{
    GCstr* str = strV(tea_lib_upvalue(T, 0));
    const char* s = str_data(str);
    uint32_t idx = (uint32_t)tea_get_number(T, tea_upvalue_index(1));

    if(str->len == 0 || idx >= str->len)
    {
        tea_push_nil(T);
        return;
    }

    /* Get the current byte */
    GCstr* chr = utf8_codepoint_at(T, str, idx);
    setstrV(T, T->top++, chr);
    
    /* Find the next index (skipping UTF-8 continuation bytes) */
    do
    {
        idx++;
        if(idx >= str->len) break;
    }
    while((s[idx] & 0xc0) == 0x80);
    
    /* Update the index */
    tea_push_number(T, idx);
    tea_replace(T, tea_upvalue_index(1));
}

static void utf8_iter(tea_State* T)
{
    tea_lib_checkstr(T, 0);
    tea_push_number(T, 0);  /* Current index */
    tea_push_cclosure(T, utf8_iternext, 2, 0, 0);
}

/* ------------------------------------------------------------------------ */

static const tea_Reg utf8_module[] = {
    { "len", utf8_len, 1, 0 },
    { "char", utf8_char, 1, 0 },
    { "ord", utf8_ord, 1, 0 },
    { "reverse", utf8_reverse, 1, 0 },
    { "iter", utf8_iter, 1, 0 },
    { NULL, NULL }
};

TEAMOD_API void tea_import_utf8(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_UTF8, utf8_module);
}