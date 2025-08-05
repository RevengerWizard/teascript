/*
** lib_string.c
** Teascript String class
*/

#include <string.h>

#define lib_string_c
#define TEA_CORE

#include "tea.h"
#include "tealib.h"

#include "tea_char.h"
#include "tea_err.h"
#include "tea_buf.h"
#include "tea_strfmt.h"
#include "tea_lib.h"

static void string_len(tea_State* T)
{
    tea_push_number(T, strV(T->base)->len);
}

static void string_init(tea_State* T)
{
    const char* str = tea_to_string(T, 1);
    tea_pop(T, 1);
    tea_push_string(T, str);
}

static void string_byte(tea_State* T)
{
    GCstr* s = tea_lib_checkstr(T, 0);
    int32_t idx = tea_lib_checkint(T, 1);
    const unsigned char* p;
    if(idx < 0 && idx > s->len)
        tea_err_msg(T, TEA_ERR_IDXSTR);
    p = (const unsigned char*)str_data(s);
    tea_push_integer(T, p[idx]);
}

static void string_upper(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    SBuf* sb = tea_buf_tmp_(T);
    int len = str->len;
    char* w = tea_buf_more(T, sb, len), *e = w + len;
    const char* q = str_data(str);
    for(; w < e; w++, q++)
    {
        uint32_t c = *(unsigned char*)q;
        *w = tea_char_toupper(c);
    }
    sb->w = w;
    setstrV(T, T->top++, tea_buf_str(T, sb));
}

static void string_lower(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    SBuf* sb = tea_buf_tmp_(T);
    int len = str->len;
    char* w = tea_buf_more(T, sb, len), *e = w + len;
    const char* q = str_data(str);
    for(; w < e; w++, q++)
    {
        uint32_t c = *(unsigned char*)q;
        *w = tea_char_tolower(c);
    }
    sb->w = w;
    setstrV(T, T->top++, tea_buf_str(T, sb));
}

static void string_reverse(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    SBuf* sb = tea_buf_tmp_(T);
    uint32_t len = str->len;
    char* w = tea_buf_more(T, sb, len), *e = w + len;
    const char* q = str_data(str) + len - 1;    
    while(w < e)
        *w++ = *q--;
    sb->w = w;
    setstrV(T, T->top++, tea_buf_str(T, sb));
}

static void string_split(tea_State* T)
{
    int count = tea_get_top(T);

    size_t len;
    char* str = (char*)tea_get_lstring(T, 0, &len);

    const char* sep = "";
    size_t sep_len = 0;
    size_t max_split = len + 1;

    if(count == 1)
    {
        sep = " ";
        sep_len = 1;
    }
    else if(count == 2)
    {
        sep = tea_check_lstring(T, 1, &sep_len);
    }
    else if(count == 3)
    {
        sep = tea_check_lstring(T, 1, &sep_len);

        if(count == 3)
        {
            max_split = tea_check_number(T, 2);
        }
    }

    tea_new_list(T, 0);
    int list_len = 0;

    SBuf* sb = tea_buf_tmp_(T);

    if(sep_len == 0)
    {
        int idx = 0;
        for(; idx < len && list_len < max_split; idx++)
        {
            list_len++;
            tea_buf_reset(sb);
            tea_buf_putmem(T, sb, str + idx, 1);
            setstrV(T, T->top++, tea_buf_str(T, sb));
            tea_add_item(T, count);
        }

        if(idx != len && list_len >= max_split)
        {
            tea_buf_reset(sb);
            tea_buf_putmem(T, sb, str + idx, len - idx);
            setstrV(T, T->top++, tea_buf_str(T, sb));
            tea_add_item(T, count);
        }
    }
    else if(max_split > 0)
    {
        const char* start = str;
        const char* found;
        
        while((found = strstr(start, sep)) && list_len < max_split)
        {
            list_len++;
            size_t part_len = found - start;
            tea_buf_reset(sb);
            tea_buf_putmem(T, sb, start, part_len);
            setstrV(T, T->top++, tea_buf_str(T, sb));
            tea_add_item(T, count);
            start = found + sep_len;
        }

        if(*start != '\0' && (list_len < max_split || found == NULL))
        {
            tea_buf_reset(sb);
            tea_buf_putmem(T, sb, start, (str + len) - start);
            setstrV(T, T->top++, tea_buf_str(T, sb));
            tea_add_item(T, count);
        }
    }
}

static void string_contains(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    GCstr* del = tea_lib_checkstr(T, 1);
    tea_push_bool(T, strstr(str_data(str), str_data(del)) != NULL);
}

static void string_startswith(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    GCstr* start = tea_lib_checkstr(T, 1);
    tea_push_bool(T, strncmp(str_data(str), str_data(start), start->len) == 0);
}

static void string_endswith(tea_State* T)
{
    GCstr* str = tea_lib_checkstr(T, 0);
    GCstr* end = tea_lib_checkstr(T, 1);
    tea_push_bool(T, strcmp(str_data(str) + (str->len - end->len), str_data(end)) == 0);
}

static void string_leftstrip(tea_State* T)
{
    size_t len;
    const char* str = tea_check_lstring(T, 0, &len);

    int i = 0;
    int count = 0;
    unsigned char c;
    for(i = 0; i < len; i++)
    {
        c = (unsigned char)(str[i]);
        if(!tea_char_isspace(c))
        {
            break;
        }
        count++;
    }

    SBuf* sb = tea_buf_tmp_(T);
    if(count != 0)
    {
        tea_buf_more(T, sb, len - count);
    }
    else
    {
        tea_buf_more(T, sb, len);
    }
    tea_buf_putmem(T, sb, str + count, len - count);
    setstrV(T, T->top++, tea_buf_str(T, sb));
}

static void string_rightstrip(tea_State* T)
{
    size_t l;
    const char* str = tea_check_lstring(T, 0, &l);

    int len;
    unsigned char c;
    for(len = l - 1; len > 0; len--)
    {
        c = (unsigned char)(str[len]);
        if(!tea_char_isspace(c))
        {
            break;
        }
    }

    SBuf* sb = tea_buf_tmp_(T);
    /* If characters were stripped resize the buffer */
    if(len + 1 != l)
    {
        tea_buf_more(T, sb, len + 1);
    }
    else
    {
        tea_buf_more(T, sb, l);
    }
    tea_buf_putmem(T, sb, str, len + 1);
    setstrV(T, T->top++, tea_buf_str(T, sb));
}

static void string_strip(tea_State* T)
{
    tea_push_cfunction(T, string_leftstrip, 1, 0);
    tea_push_value(T, 0);
    tea_call(T, 1);

    tea_push_cfunction(T, string_rightstrip, 1, 0);
    tea_push_value(T, 1);
    tea_call(T, 1);
}

static void string_count(tea_State* T)
{
    const char* str = tea_check_string(T, 0);
    const char* needle = tea_check_string(T, 1);
    int count = 0;
    while((str = strstr(str, needle)))
    {
        count++;
        str++;
    }
    tea_push_number(T, count);
}

static void string_find(tea_State* T)
{
    int count = tea_get_top(T);
    int idx = 1;
    if(count == 3)
    {
        idx = tea_check_number(T, 2);
    }

    size_t len;
    const char* str = tea_check_string(T, 0);
    const char* substr = tea_check_lstring(T, 1, &len);

    int pos = 0;
    for(int i = 0; i < idx; i++)
    {
        char* result = strstr(str, substr);
        if(!result)
        {
            pos = -1;
            break;
        }

        pos += (result - str) + (i * len);
        str = result + len;
    }

    tea_push_number(T, pos);
}

static void string_replace(tea_State* T)
{
    size_t len, slen, rlen;
    const char* str = tea_check_lstring(T, 0, &len);
    const char* search = tea_check_lstring(T, 1, &slen);
    const char* replace = tea_check_lstring(T, 2, &rlen);

    if(len == 0 && slen == 0)
    {
        return;
    }

    if(slen == 0)
    {
        tea_pop(T, 2);
        return;
    }

    if(strstr(str, search) == NULL)
    {
        tea_pop(T, 2);
        return;
    }

    if(str == search)
    {
        return;
    }

    size_t result_size = len;
    for(const char* p = str; (p = strstr(p, search)); p += slen)
    {
        result_size += rlen - slen;
    }

    SBuf* sb = tea_buf_tmp_(T);
    tea_buf_more(T, sb, result_size);

    /* Perform the replacement */
    for(const char* p = str; (p = strstr(p, search)); p += slen)
    {
        size_t n = p - str;
        tea_buf_putmem(T, sb, str, n);
        tea_buf_putmem(T, sb, replace, rlen);
        str = p + slen;
    }
    tea_buf_putmem(T, sb, str, strlen(str));
    setstrV(T, T->top++, tea_buf_str(T, sb));
}

static void string_format(tea_State* T)
{
    int retry = 0;
    SBuf* sb;
    do
    {
        sb = tea_buf_tmp_(T);
        retry = tea_strfmt_putarg(T, sb, 0, -retry);
    }
    while(retry > 0);
    setstrV(T, T->top - 1, tea_buf_str(T, sb));
}

static void string_repeat(tea_State* T)
{
    GCstr* s = tea_lib_checkstr(T, 0);
    int32_t rep = tea_lib_checkint(T, 1);
    GCstr* sep = tea_lib_optstr(T, 2);
    SBuf* sb = tea_buf_tmp_(T);
    if(sep && rep > 1)
    {
        GCstr* s2 = tea_buf_cat2str(T, sep, s);
        tea_buf_reset(sb);
        s = s2;
        rep--;
    }
    sb = tea_buf_putstr_repeat(T, sb, s, rep);
    setstrV(T, T->top - 1, tea_buf_str(T, sb));
}

static void string_iternext(tea_State* T)
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
    char byte = s[idx];
    /* Create a single-byte string */
    GCstr* c = tea_str_new(T, &byte, 1);
    setstrV(T, T->top++, c);
    
    /* Move to the next byte */
    idx++;
    
    /* Update the index */
    tea_push_number(T, idx);
    tea_replace(T, tea_upvalue_index(1));
}

static void string_iter(tea_State* T)
{
    tea_lib_checkstr(T, 0);
    tea_push_number(T, 0);  /* Current index */
    tea_push_cclosure(T, string_iternext, 2, 0, 0);
}

static void string_opadd(tea_State* T)
{
    if(!tvisstr(T->base) || !tvisstr(T->base + 1))
        tea_err_bioptype(T, T->base, T->base + 1, MM_PLUS);

    GCstr* s1 = tea_lib_checkstr(T, 0);
    GCstr* s2 = tea_lib_checkstr(T, 1);
    GCstr* str = tea_buf_cat2str(T, s1, s2);
    T->top -= 2;
    setstrV(T, T->top++, str);
}

/* ------------------------------------------------------------------------ */

static const tea_Methods string_reg[] = {
    { "len", "getter", string_len, 1, 0 },
    { "new", "method", string_init, 2, 0 },
    { "byte", "method", string_byte, 2, 0 },
    { "upper", "method", string_upper, 1, 0 },
    { "lower", "method", string_lower, 1, 0 },
    { "reverse", "method", string_reverse, 1, 0 },
    { "split", "method", string_split, 1, 2 },
    { "contains", "method", string_contains, 2, 0 },
    { "startswith", "method", string_startswith, 2, 0 },
    { "endswith", "method", string_endswith, 2, 0 },
    { "leftstrip", "method", string_leftstrip, 1, 0 },
    { "rightstrip", "method", string_rightstrip, 1, 0 },
    { "strip", "method", string_strip, 1, 0 },
    { "count", "method", string_count, 2, 0 },
    { "find", "method", string_find, 2, 1 },
    { "replace", "method", string_replace, 3, 0 },
    { "format", "method", string_format, TEA_VARG, 0 },
    { "repeat", "method", string_repeat, 2, 1 },
    { "iter", "method", string_iter, 1, 0 },
    { "+", "static", string_opadd, 2, 0 },
    { NULL, NULL, NULL }
};

void tea_open_string(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_STRING, string_reg);
    T->gcroot[GCROOT_KLSTR] = obj2gco(classV(T->top - 1));
    tea_set_global(T, TEA_CLASS_STRING);
    tea_push_nil(T);
}