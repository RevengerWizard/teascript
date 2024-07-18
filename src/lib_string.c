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
#include "tea_vm.h"
#include "tea_gc.h"
#include "tea_utf.h"
#include "tea_str.h"
#include "tea_buf.h"
#include "tea_strfmt.h"
#include "tea_lib.h"

static void string_len(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_push_number(T, tea_utf_len(strV(T->base)));
}

static void string_init(tea_State* T)
{
    const char* str = tea_to_string(T, 1);
    tea_pop(T, 1);
    tea_push_string(T, str);
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
    setstrV(T, T->top++, tea_utf_reverse(T, str));
}

static void string_split(tea_State* T)
{
    int count = tea_get_top(T);

    size_t len;
    char* str = (char*)tea_get_lstring(T, 0, &len);

    const char* sep = "";
    size_t sep_len;
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

    char* temp = tea_mem_newvec(T, char, len + 1);
    char* temp_free = temp;
    memcpy(temp, str, len);
    temp[len] = '\0';

    char* token;

    tea_new_list(T);
    int list_len = 0;

    if(sep_len == 0)
    {
        int idx = 0;
        for(; idx < len && list_len < max_split; idx++)
        {
            list_len++;
            *(temp) = str[idx];
            *(temp + 1) = '\0';

            tea_push_string(T, temp);
            tea_add_item(T, count);
        }

        if(idx != len && list_len >= max_split)
        {
            temp = str + idx;
        }
        else
        {
            temp = NULL;
        }
    }
    else if(max_split > 0)
    {
        do
        {
            list_len++;
            token = strstr(temp, sep);
            if(token)
            {
                *token = '\0';
            }

            tea_push_string(T, temp);
            tea_add_item(T, count);
            temp = token + sep_len;
        }
        while(token != NULL && list_len < max_split);

        if(token == NULL)
        {
            temp = NULL;
        }
    }

    if(temp != NULL && list_len >= max_split)
    {
        tea_push_string(T, temp);
        tea_add_item(T, count);
    }
    tea_mem_freevec(T, char, temp_free, len + 1);
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
    tea_push_cfunction(T, string_leftstrip, 1);
    tea_push_value(T, 0);
    tea_call(T, 1);

    tea_push_cfunction(T, string_rightstrip, 1);
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

static void string_iterate(tea_State* T)
{
    size_t len;
    const char* str = tea_check_lstring(T, 0, &len);

	if(tea_is_nil(T, 1))
    {
		if(len == 0)
        {
            tea_push_nil(T);
            return;
        }
		tea_push_number(T, 0);
        return;
	}

    int idx = tea_check_number(T, 1);
	if(idx < 0)
    {
        tea_push_nil(T);
        return;
    }

	do
    {
		idx++;
		if(idx >= len)
        {
            tea_push_nil(T);
            return;
        }
	}
    while((str[idx] & 0xc0) == 0x80);

	tea_push_number(T, idx);
}

static void string_iteratorvalue(tea_State* T)
{
	int idx = tea_check_number(T, 1);
    GCstr* s = tea_utf_codepoint_at(T, strV(T->base), idx);
    setstrV(T, T->top++, s);
}

static void string_opadd(tea_State* T)
{
    GCstr* s1 = tea_lib_checkstr(T, 0);
    GCstr* s2 = tea_lib_checkstr(T, 1);
    GCstr* str = tea_buf_cat2str(T, s1, s2);
    T->top -= 2;
    setstrV(T, T->top++, str);
}

static bool repeat(tea_State* T)
{
    GCstr* str;
    int n;

    if(tvisstr(T->top - 1) && tvisnum(T->top - 2))
    {
        str = strV(T->top - 1);
        n = numV(T->top - 2);
    }
    else if(tvisnum(T->top - 1) && tvisstr(T->top - 2))
    {
        n = numV(T->top - 1);
        str = strV(T->top - 2);
    }
    else
    {
        return false;
    }

    if(n <= 0)
    {
        T->top -= 2;
        setstrV(T, T->top++, &T->strempty);
        return true;
    }
    else if(n == 1)
    {
        T->top -= 2;
        setstrV(T, T->top++, str);
        return true;
    }

    int len = str->len;
    char* chars = tea_buf_tmp(T, n * len);

    int i;
    char* p;
    for(i = 0, p = chars; i < n; ++i, p += len)
    {
        memcpy(p, str_data(str), len);
    }

    GCstr* result = tea_str_new(T, chars, n * len);
    T->top -= 2;
    setstrV(T, T->top++, result);
    return true;
}

static void string_opmultiply(tea_State* T)
{
    if(!repeat(T))
    {
        tea_error(T, "string multiply error");
    }
}

/* ------------------------------------------------------------------------ */

static const tea_Methods string_class[] = {
    { "len", "property", string_len, TEA_VARARGS },
    { "new", "method", string_init, 2 },
    { "upper", "method", string_upper, 1 },
    { "lower", "method", string_lower, 1 },
    { "reverse", "method", string_reverse, 1 },
    { "split", "method", string_split, -3 },
    { "contains", "method", string_contains, 2 },
    { "startswith", "method", string_startswith, 2 },
    { "endswith", "method", string_endswith, 2 },
    { "leftstrip", "method", string_leftstrip, 1 },
    { "rightstrip", "method", string_rightstrip, 1 },
    { "strip", "method", string_strip, 1 },
    { "count", "method", string_count, 2 },
    { "find", "method", string_find, -3 },
    { "replace", "method", string_replace, 3 },
    { "format", "method", string_format, TEA_VARARGS },
    { "iterate", "method", string_iterate, 2 },
    { "iteratorvalue", "method", string_iteratorvalue, 2 },
    { "+", "static", string_opadd, 2 },
    { "*", "static", string_opmultiply, 2 },
    { NULL, NULL, NULL }
};

void tea_open_string(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_STRING, string_class);
    T->string_class = classV(T->top - 1);
    tea_set_global(T, TEA_CLASS_STRING);
    tea_push_nil(T);
}