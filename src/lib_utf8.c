/*
** lib_utf8.c
** Teascript UTF-8 module
*/

#define lib_utf8_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_obj.h"
#include "tea_utf.h"
#include "tea_lib.h"

static void utf8_len(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    tea_push_number(T, tea_utf_len(str));
}

static void utf8_char(tea_State* T)
{
    uint32_t n = (uint32_t)tea_lib_checkint(T, 0);
    setstrV(T, T->top++, tea_utf_from_codepoint(T, n));
}

static void utf8_ord(tea_State* T)
{
    GCstr* c = tea_lib_checkstr(T, 0);
    tea_push_number(T, tea_utf_decode((uint8_t*)str_data(c), c->len));
}

static void utf8_reverse(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    setstrV(T, T->top++, tea_utf_reverse(T, str));
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
    GCstr* chr = tea_utf_codepoint_at(T, str, idx);
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
    { "reverse", utf8_reverse, 0, 0 },
    { "iter", utf8_iter, 1, 0 },
    { NULL, NULL }
};

TEAMOD_API void tea_import_utf8(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_UTF8, utf8_module);
}