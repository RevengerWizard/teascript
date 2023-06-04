/*
** tea_stringclass.c
** Teascript string class
*/

#include <string.h>
#include <ctype.h>

#define tea_stringclass_c
#define TEA_CORE

#include "tea.h"

#include "tea_vm.h"
#include "tea_memory.h"
#include "tea_core.h"
#include "tea_utf.h"
#include "tea_string.h"

static void string_len(TeaState* T)
{
    tea_push_number(T, tea_utf_length(AS_STRING(T->base[0])));
}

static void string_constructor(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);
    const char* string = tea_to_string(T, 1);
    tea_pop(T, 1);
    tea_push_string(T, string);
}

static void string_upper(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);

    int len;
    const char* string = tea_get_lstring(T, 0, &len);
    char* temp = TEA_ALLOCATE(T, char, len + 1);

    for(int i = 0; string[i]; i++) 
    {
        temp[i] = toupper(string[i]);
    }
    temp[len] = '\0';

    tea_vm_push(T, OBJECT_VAL(tea_string_take(T, temp, len)));
}

static void string_lower(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);

    int len;
    const char* string = tea_get_lstring(T, 0, &len);
    char* temp = TEA_ALLOCATE(T, char, len + 1);

    for(int i = 0; string[i]; i++) 
    {
        temp[i] = tolower(string[i]);
    }
    temp[len] = '\0';

    tea_vm_push(T, OBJECT_VAL(tea_string_take(T, temp, len)));
}

static void rev(char* str, int len)
{
    // this assumes that str is valid UTF-8
    char* scanl, *scanr, *scanr2, c;

    // first reverse the string
    for(scanl = str, scanr = str + len; scanl < scanr;)
        c = *scanl, *scanl++ = *--scanr, *scanr = c;

    // then scan all bytes and reverse each multibyte character
    for(scanl = scanr = str; (c = *scanr++);)
    {
        if((c & 0x80) == 0) // ASCII char
            scanl = scanr;
        else if((c & 0xc0) == 0xc0)
        { // start of multibyte
            scanr2 = scanr;
            switch(scanr - scanl)
            {
                case 4:
                    c = *scanl, *scanl++ = *--scanr, *scanr = c; // fallthrough
                case 3:                                          // fallthrough
                case 2:
                    c = *scanl, *scanl++ = *--scanr, *scanr = c;
            }
            scanr = scanl = scanr2;
        }
    }
}

static void string_reverse(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);

    int len;
    char* string = (char*)tea_get_lstring(T, 0, &len);
    if(len < 2)
    {
        return;
    }

    char* reversed = TEA_ALLOCATE(T, char, len + 1);
    strcpy(reversed, string);
    rev(reversed, len);
    tea_vm_push(T, OBJECT_VAL(tea_string_take(T, reversed, len)));
}

static void string_split(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 3, "Expected 0 to 2 arguments, got %d", count);

    int len;
    char* string = (char*)tea_get_lstring(T, 0, &len);

    const char* sep;
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

    char* temp = TEA_ALLOCATE(T, char, len + 1);
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
    TEA_FREE_ARRAY(T, char, temp_free, len + 1);
}

static void string_title(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);

    int len;
    const char* string = tea_get_lstring(T, 0, &len);
    char* temp = TEA_ALLOCATE(T, char, len + 1);

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

    tea_vm_push(T, OBJECT_VAL(tea_string_take(T, temp, len)));
}

static void string_contains(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 2);

    const char* string = tea_get_string(T, 0);
    const char* delimiter = tea_check_string(T, 1);

    tea_push_bool(T, strstr(string, delimiter) != NULL);
}

static void string_startswith(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 2);

    const char* string = tea_get_string(T, 0);
    int len;
    const char* start = tea_check_lstring(T, 1, &len);

    tea_push_bool(T, strncmp(string, start, len) == 0);
}

static void string_endswith(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 2);

    int l1, l2;
    const char* string = tea_get_lstring(T, 0, &l1);
    const char* end = tea_check_lstring(T, 1, &l2);

    tea_push_bool(T, strcmp(string + (l1 - l2), end) == 0);
}

static void string_leftstrip(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);

    int len;
    const char* string = tea_get_lstring(T, 0, &len);

    int i = 0;
    count = 0;
    char* temp = TEA_ALLOCATE(T, char, len + 1);

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
        temp = TEA_GROW_ARRAY(T, char, temp, len + 1, (len - count) + 1);
    }

    memcpy(temp, string + count, len - count);
    temp[len - count] = '\0';

    tea_vm_push(T, OBJECT_VAL(tea_string_take(T, temp, len - count)));
}

static void string_rightstrip(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);

    int l;
    const char* string = tea_get_lstring(T, 0, &l);

    int length;
    char* temp = TEA_ALLOCATE(T, char, l + 1);

    for(length = l - 1; length > 0; length--) 
    {
        if(!isspace(string[length]))
        {
            break;
        }
    }

    // If characters were stripped resize the buffer
    if(length + 1 != l) 
    {
        temp = TEA_GROW_ARRAY(T, char, temp, l + 1, length + 2);
    }

    memcpy(temp, string, length + 1);
    temp[length + 1] = '\0';

    tea_vm_push(T, OBJECT_VAL(tea_string_take(T, temp, length + 1)));
}

static void string_strip(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);

    tea_push_cfunction(T, string_leftstrip);
    tea_push_value(T, 0);
    tea_call(T, 1);

    tea_push_cfunction(T, string_rightstrip);
    tea_push_value(T, 1);
    tea_call(T, 1);
}

static void string_count(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    const char* string = tea_get_string(T, 0);
    const char* needle = tea_check_string(T, 1);

    count = 0;
    while((string = strstr(string, needle))) 
    {
        count++;
        string++;
    }

    tea_push_number(T, count);
}

static void string_find(TeaState* T)
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

static void string_replace(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 3);

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

    char* result = TEA_ALLOCATE(T, char, result_size + 1);

    // Perform the replacement
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

    tea_vm_push(T, OBJECT_VAL(tea_string_take(T, result, result_size)));
}

static void string_iterate(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 2);

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

static void string_iteratorvalue(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 2);

	int index = tea_check_number(T, 1);

    tea_vm_push(T, OBJECT_VAL(tea_utf_code_point_at(T, AS_STRING(T->base[0]), index)));
}

static const TeaClass string_class[] = {
    { "len", "property", string_len },
    { "constructor", "method", string_constructor },
    { "upper", "method", string_upper },
    { "lower", "method", string_lower },
    { "reverse", "method", string_reverse },
    { "split", "method", string_split },
    { "title", "method", string_title },
    { "contains", "method", string_contains },
    { "startswith", "method", string_startswith },
    { "endswith", "method", string_endswith },
    { "leftstrip", "method", string_leftstrip },
    { "rightstrip", "method", string_rightstrip },
    { "strip", "method", string_strip },
    { "count", "method", string_count },
    { "find", "method", string_find },
    { "replace", "method", string_replace },
    { "iterate", "method", string_iterate },
    { "iteratorvalue", "method", string_iteratorvalue },
    { NULL, NULL, NULL }
};

void tea_open_string(TeaState* T)
{
    tea_create_class(T, TEA_STRING_CLASS, string_class);
    T->string_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_STRING_CLASS);
    tea_push_null(T);
}