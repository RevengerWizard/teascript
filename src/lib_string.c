/*
** lib_string.c
** Teascript String class
*/

#define lib_string_c
#define TEA_CORE

#include <string.h>
#include <ctype.h>

#include "tea.h"
#include "tealib.h"

#include "tea_vm.h"
#include "tea_gc.h"
#include "tea_utf.h"
#include "tea_str.h"
#include "tea_buf.h"
#include "tea_strfmt.h"

static void string_len(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_push_number(T, tea_utf_len(strV(T->base)));
}

static void string_constructor(tea_State* T)
{
    const char* string = tea_to_string(T, 1);
    tea_pop(T, 1);
    tea_push_string(T, string);
}

static void string_upper(tea_State* T)
{
    int len;
    const char* string = tea_get_lstring(T, 0, &len);
    char* temp = tea_mem_new(T, char, len + 1);

    for(int i = 0; string[i]; i++)
    {
        temp[i] = toupper(string[i]);
    }
    temp[len] = '\0';

    GCstr* s = tea_str_take(T, temp, len);
    setstrV(T, T->top++, s);
}

static void string_lower(tea_State* T)
{
    int len;
    const char* string = tea_get_lstring(T, 0, &len);
    char* temp = tea_mem_new(T, char, len + 1);

    for(int i = 0; string[i]; i++)
    {
        temp[i] = tolower(string[i]);
    }
    temp[len] = '\0';

    GCstr* s = tea_str_take(T, temp, len);
    setstrV(T, T->top++, s);
}

static void string_reverse(tea_State* T)
{
    tea_check_string(T, 0);

    GCstr* string = strV(T->base);
    GCstr* reversed = tea_utf_reverse(T, string);

    setstrV(T, T->top++, reversed);
}

static void string_split(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 3, "Expected 0 to 2 arguments, got %d", count);

    int len;
    char* string = (char*)tea_get_lstring(T, 0, &len);

    const char* sep = "";
    int sep_len;
    int max_split = len + 1;

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

    char* temp = tea_mem_new(T, char, len + 1);
    char* temp_free = temp;
    memcpy(temp, string, len);
    temp[len] = '\0';

    char* token;

    tea_new_list(T);
    int list_len = 0;

    if(sep_len == 0)
    {
        int index = 0;
        for(; index < len && list_len < max_split; index++)
        {
            list_len++;
            *(temp) = string[index];
            *(temp + 1) = '\0';

            tea_push_string(T, temp);
            tea_add_item(T, count);
        }

        if(index != len && list_len >= max_split)
        {
            temp = string + index;
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

static void string_title(tea_State* T)
{
    int len;
    const char* string = tea_get_lstring(T, 0, &len);
    char* temp = tea_mem_new(T, char, len + 1);

    bool next = true;
    for(int i = 0; string[i]; i++)
    {
        if(string[i] == ' ')
        {
            next = true;
        }
        else if(next)
        {
            temp[i] = toupper(string[i]);
            next = false;
            continue;
        }
        temp[i] = tolower(string[i]);
    }
    temp[len] = '\0';

    GCstr* s = tea_str_take(T, temp, len);
    setstrV(T, T->top++, s);
}

static void string_contains(tea_State* T)
{
    const char* string = tea_get_string(T, 0);
    const char* delimiter = tea_check_string(T, 1);

    tea_push_bool(T, strstr(string, delimiter) != NULL);
}

static void string_startswith(tea_State* T)
{
    const char* string = tea_get_string(T, 0);
    int len;
    const char* start = tea_check_lstring(T, 1, &len);

    tea_push_bool(T, strncmp(string, start, len) == 0);
}

static void string_endswith(tea_State* T)
{
    int l1, l2;
    const char* string = tea_get_lstring(T, 0, &l1);
    const char* end = tea_check_lstring(T, 1, &l2);

    tea_push_bool(T, strcmp(string + (l1 - l2), end) == 0);
}

static void string_leftstrip(tea_State* T)
{
    int len;
    const char* string = tea_get_lstring(T, 0, &len);

    int i = 0;
    int count = 0;
    char* temp = tea_mem_new(T, char, len + 1);

    for(i = 0; i < len; i++)
    {
        if(!isspace(string[i]))
        {
            break;
        }
        count++;
    }

    if(count != 0)
    {
        temp = tea_mem_reallocvec(T, char, temp, len + 1, (len - count) + 1);
    }

    memcpy(temp, string + count, len - count);
    temp[len - count] = '\0';

    GCstr* s = tea_str_take(T, temp, len - count);
    setstrV(T, T->top++, s);
}

static void string_rightstrip(tea_State* T)
{
    int l;
    const char* string = tea_get_lstring(T, 0, &l);

    int len;
    char* temp = tea_mem_new(T, char, l + 1);

    for(len = l - 1; len > 0; len--)
    {
        if(!isspace(string[len]))
        {
            break;
        }
    }

    /* If characters were stripped resize the buffer */
    if(len + 1 != l)
    {
        temp = tea_mem_reallocvec(T, char, temp, l + 1, len + 2);
    }

    memcpy(temp, string, len + 1);
    temp[len + 1] = '\0';

    GCstr* s = tea_str_take(T, temp, len + 1);
    setstrV(T, T->top++, s);
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
    const char* string = tea_get_string(T, 0);
    const char* needle = tea_check_string(T, 1);

    int count = 0;
    while((string = strstr(string, needle)))
    {
        count++;
        string++;
    }

    tea_push_number(T, count);
}

static void string_find(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 3, "Expected 1 or 2 arguments, got %d", count);

    int index = 1;
    if(count == 3)
    {
        index = tea_check_number(T, 2);
    }

    int len;
    const char* string = tea_get_string(T, 0);
    const char* substr = tea_check_lstring(T, 1, &len);

    int position = 0;
    for(int i = 0; i < index; i++)
    {
        char* result = strstr(string, substr);
        if(!result)
        {
            position = -1;
            break;
        }

        position += (result - string) + (i * len);
        string = result + len;
    }

    tea_push_number(T, position);
}

static void string_replace(tea_State* T)
{
    int len, slen, rlen;
    const char* string = tea_check_lstring(T, 0, &len);
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

    if(strstr(string, search) == NULL)
    {
        tea_pop(T, 2);
        return;
    }

    if(string == search)
    {
        return;
    }

    size_t result_size = len;
    for(const char* p = string; (p = strstr(p, search)); p += slen)
    {
        result_size += rlen - slen;
    }

    char* result = tea_mem_new(T, char, result_size + 1);

    /* Perform the replacement */
    char* q = result;
    for(const char* p = string; (p = strstr(p, search)); p += slen)
    {
        size_t n = p - string;
        memcpy(q, string, n);
        q += n;
        memcpy(q, replace, rlen);
        q += rlen;
        string = p + slen;
    }
    strcpy(q, string);

    GCstr* s = tea_str_take(T, result, result_size);
    setstrV(T, T->top++, s);
}

static void string_format(tea_State* T)
{
    SBuf* sb;
    sb = tea_buf_tmp_(T);
    tea_strfmt_putarg(T, sb, 0);
    setstrV(T, T->top - 1, tea_buf_str(T, sb));
}

static void string_iterate(tea_State* T)
{
    int len;
    const char* string = tea_get_lstring(T, 0, &len);

	if(tea_is_null(T, 1))
    {
		if(len == 0)
        {
            tea_push_null(T);
            return;
        }
		tea_push_number(T, 0);
        return;
	}

    int index = tea_check_number(T, 1);
	if(index < 0)
    {
        tea_push_null(T);
        return;
    }

	do
    {
		index++;
		if(index >= len)
        {
            tea_push_null(T);
            return;
        }
	}
    while((string[index] & 0xc0) == 0x80);

	tea_push_number(T, index);
}

static void string_iteratorvalue(tea_State* T)
{
	int index = tea_check_number(T, 1);

    GCstr* s = tea_utf_codepoint_at(T, strV(T->base), index);
    setstrV(T, T->top++, s);
}

static void string_opadd(tea_State* T)
{
    tea_check_string(T, 0);
    tea_check_string(T, 1);

    GCstr* s1 = strV(T->top - 2);
    GCstr* s2 = strV(T->top - 1);

    GCstr* str = tea_buf_cat2str(T, s1, s2);

    T->top -= 2;
    setstrV(T, T->top++, str);
}

static bool repeat(tea_State* T)
{
    GCstr* string;
    int n;

    if(tvisstr(T->top - 1) && tvisnumber(T->top - 2))
    {
        string = strV(T->top - 1);
        n = numberV(T->top - 2);
    }
    else if(tvisnumber(T->top - 1) && tvisstr(T->top - 2))
    {
        n = numberV(T->top - 1);
        string = strV(T->top - 2);
    }
    else
    {
        return false;
    }

    if(n <= 0)
    {
        GCstr* s = tea_str_newlit(T, "");
        T->top -= 2;
        setstrV(T, T->top++, s);
        return true;
    }
    else if(n == 1)
    {
        T->top -= 2;
        setstrV(T, T->top++, string);
        return true;
    }

    int len = string->len;
    char* chars = tea_mem_new(T, char, (n * len) + 1);

    int i;
    char* p;
    for(i = 0, p = chars; i < n; ++i, p += len)
    {
        memcpy(p, string->chars, len);
    }
    *p = '\0';

    GCstr* result = tea_str_take(T, chars, strlen(chars));
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

static const tea_Class string_class[] = {
    { "len", "property", string_len, TEA_VARARGS },
    { "constructor", "method", string_constructor, 2 },
    { "upper", "method", string_upper, 1 },
    { "lower", "method", string_lower, 1 },
    { "reverse", "method", string_reverse, 1 },
    { "split", "method", string_split, TEA_VARARGS },
    { "title", "method", string_title, 1 },
    { "contains", "method", string_contains, 2 },
    { "startswith", "method", string_startswith, 2 },
    { "endswith", "method", string_endswith, 2 },
    { "leftstrip", "method", string_leftstrip, 1 },
    { "rightstrip", "method", string_rightstrip, 1 },
    { "strip", "method", string_strip, 1 },
    { "count", "method", string_count, 2 },
    { "find", "method", string_find, TEA_VARARGS },
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
    tea_push_null(T);
}