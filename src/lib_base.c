/*
** lib_core.c
** Teascript base classes and functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define lib_base_c
#define TEA_CORE

#include "tea.h"
#include "tealib.h"

#include "tea_char.h"
#include "tea_bcdump.h"
#include "tea_buf.h"
#include "tea_vm.h"
#include "tea_str.h"
#include "tea_gc.h"
#include "tea_utf.h"
#include "tea_strfmt.h"
#include "tea_strscan.h"
#include "tea_lib.h"

static void base_print(tea_State* T)
{
    int count = tea_get_top(T);
    for(int i = 0; i < count; i++)
    {
        size_t len;
        const char* str = tea_to_lstring(T, i, &len);
        if(i)
            putchar('\t');

        fwrite(str, sizeof(char), len, stdout);
        tea_pop(T, 1);
    }
    putchar('\n');
    tea_push_nil(T);
}

static void base_input(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count > 1, "Expected 0 or 1 arguments, got %d", count);
    if(count != 0)
    {
        GCstr* str = tea_lib_checkstr(T, 0);
        fwrite(str_data(str), sizeof(char), str->len, stdout);
    }

    size_t m = TEA_BUFFER_SIZE, n = 0, ok = 0;
    char* buf;
    while(true)
    {
        buf = tea_buf_tmp(T, m);
        if(fgets(buf + n, m - n, stdin) == NULL)
            break;
        n += strlen(buf + n);
        ok |= n;
        if(n && buf[n - 1] == '\n') { n -= 1; break; }
        if(n >= m - 64) m += m;
    }
    setstrV(T, T->top++, tea_str_new(T, buf, n));
    if(!ok)
        setnilV(T->top - 1);
}

static void base_assert(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1, "Expected at least 1 argument, got %d", count);
    if(!tea_to_bool(T, 0))
    {
        tea_error(T, "%s", tea_opt_string(T, 1, "assertion failed!"));
    }
    tea_push_value(T, 0);
}

static void base_error(tea_State* T)
{
    tea_error(T, "%s", tea_check_string(T, 0));
    tea_push_nil(T);
}

static void base_typeof(tea_State* T)
{
    tea_push_string(T, tea_typeof(T, 0));
}

static void base_gc(tea_State* T)
{
    int collected = tea_gc(T);
    tea_push_number(T, collected);
}

static void base_eval(tea_State* T)
{
    size_t len;
    const char* s = tea_check_lstring(T, 0, &len);
    if(tea_load_buffer(T, s, len, "?<interpret>") == TEA_OK)
    {
        tea_call(T, 0);
    }
    tea_push_nil(T);
}

static int writer_buf(tea_State* T, void* sb, const void* p, size_t size)
{
    tea_buf_putmem(T, (SBuf*)sb, p, size);
    UNUSED(T);
    return 0;
}

static void base_dump(tea_State* T)
{
    tea_check_function(T, 0);
    GCfunc* fn = funcV(T->base);
    SBuf* sb = tea_buf_tmp_(T);
    if(!isteafunc(fn) || tea_bcwrite(T, fn->t.proto, writer_buf, sb))
    {
        tea_error(T, "Unable to dump given function");
    }
    setstrV(T, T->top++, tea_buf_str(T, sb));
}

static void base_loadfile(tea_State* T)
{
    const char* fname = tea_check_string(T, 0);
    const char* mode = tea_opt_string(T, 1, "t");
    int status = tea_load_filex(T, fname, NULL, mode);
    if(status != TEA_OK)
    {
        tea_error(T, "Unable to load file");
    }
}

static void base_loadstring(tea_State* T)
{
    size_t len;
    const char* str = tea_check_lstring(T, 0, &len);
    const char* name = tea_opt_string(T, 1, "b");
    int status = tea_load_bufferx(T, str, len, name ? name : "?<load>", NULL);
    if(status != TEA_OK)
    {
        tea_error(T, "Unable to load string");
    }
}

static void base_char(tea_State* T)
{
    int n = tea_check_number(T, 0);
    setstrV(T, T->top++, tea_utf_from_codepoint(T, n));
}

static void base_ord(tea_State* T)
{
    GCstr* c = tea_lib_checkstr(T, 0);
    tea_push_number(T, tea_utf_decode((uint8_t*)str_data(c), c->len));
}

static void base_hex(tea_State* T)
{
    double n = tea_check_number(T, 0);
    tea_strfmt_pushf(T, "0x%x", (unsigned int)n);
}

static void base_bin(tea_State* T)
{
    int n = tea_check_number(T, 0);

    char buffer[34];
    int i = 2;

    buffer[0] = '0';
    buffer[1] = 'b';

    while(n > 0)
    {
        buffer[i++] = (n % 2) + '0';
        n /= 2;
    }
    if(i == 2)
    {
        buffer[i++] = '0';
    }

    buffer[i] = '\0';

    /* Reverse the buffer to get the binary representation in the correct order */
    char temp;
    for(int j = 0; j < (i - 2) / 2; j++)
    {
        temp = buffer[j + 2];
        buffer[j + 2] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }

    tea_push_string(T, buffer);
}

static void base_rawequal(tea_State* T)
{
    cTValue* o1 = tea_lib_checkany(T, 0);
    cTValue* o2 = tea_lib_checkany(T, 1);
    tea_push_bool(T, tea_obj_rawequal(o1, o2));
}

static void base_hasattr(tea_State* T)
{
    tea_check_any(T, 0);
    const char* attr = tea_check_string(T, 1);
    bool found = tea_get_attr(T, 0, attr);
    tea_push_bool(T, found);
}

static void base_getattr(tea_State* T)
{
    tea_check_any(T, 0);
    const char* attr = tea_check_string(T, 1);
    bool found = tea_get_attr(T, 0, attr);
    if(!found)
        tea_error(T, TEA_QS " has no attribute " TEA_QS, tea_typename(T->base), attr);
}

static void base_setattr(tea_State* T)
{
    tea_check_any(T, 0);
    const char* attr = tea_check_string(T, 1);
    tea_check_any(T, 2);
    tea_set_attr(T, 0, attr);
    T->top--;
}

static void bool_init(tea_State* T)
{
    bool b = tea_to_bool(T, 1);
    tea_push_bool(T, b);
}

static void number_init(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count > 3, "Expected at least 2 argument, got %d", count);
    int base = tea_opt_number(T, 2, 10);
    if(base == 10)
    {
        bool is_num;
        double n = tea_to_numberx(T, 1, &is_num);
        if(is_num)
        {
            tea_push_number(T, n);
            return;
        }
    }
    else
    {
        const char* p = str_data(tea_lib_checkstr(T, 1));
        char* ep;
        unsigned int neg = 0;
        unsigned long ul;
        if(base < 2 || base > 36)
            tea_error(T, "Number base out of range");
        while(tea_char_isspace((unsigned char)(*p))) p++;
        if(*p == '-') { p++; neg = 1; } else if(*p == '+') { p++; }
        if(tea_char_isalnum((unsigned char)(*p)))
        {
            ul = strtoul(p, &ep, base);
            if(p != ep)
            {
                while(tea_char_isspace((unsigned char)(*ep))) ep++;
                if(*ep == '\0')
                {

                    double n = (double)ul;
                    if(neg) n = -n;
                    tea_push_number(T, n);
                    return;
                }
            }
        }
    }
    tea_push_nil(T);
}

static void invalid_init(tea_State* T)
{
    tea_error(T, "Invalid init");
}

/* ------------------------------------------------------------------------ */

static const tea_Methods func_class[] = {
    { "init", "method", invalid_init, TEA_VARARGS },
    { NULL, NULL }
};

static const tea_Methods number_class[] = {
    { "init", "method", number_init, TEA_VARARGS },
    { NULL, NULL }
};

static const tea_Methods bool_class[] = {
    { "init", "method", bool_init, 2 },
    { NULL, NULL }
};

static const tea_Reg globals[] = {
    { "print", base_print, TEA_VARARGS },
    { "input", base_input, TEA_VARARGS },
    { "assert", base_assert, TEA_VARARGS },
    { "error", base_error, 1 },
    { "typeof", base_typeof, 1 },
    { "gc", base_gc, 0 },
    { "eval", base_eval, 1 },
    { "dump", base_dump, 1 },
    { "loadfile", base_loadfile, 1 },
    { "loadstring", base_loadstring, 1 },
    { "char", base_char, 1 },
    { "ord", base_ord, 1 },
    { "hex", base_hex, 1 },
    { "bin", base_bin, 1 },
    { "rawequal", base_rawequal, 2 },
    { "hasattr", base_hasattr, 2 },
    { "getattr", base_getattr, 2 },
    { "setattr", base_setattr, 3 },
    { NULL, NULL }
};

static void tea_open_global(tea_State* T)
{
    const tea_Reg* reg = globals;
    for(; reg->name; reg++)
    {
        tea_push_cfunction(T, reg->fn, reg->nargs);
        tea_set_global(T, reg->name);
    }

    tea_create_class(T, "Number", number_class);
    T->number_class = classV(T->top - 1);
    tea_set_global(T, "Number");
    tea_create_class(T, "Bool", bool_class);
    T->bool_class = classV(T->top - 1);
    tea_set_global(T, "Bool");
    tea_create_class(T, "Function", func_class);
    T->func_class = classV(T->top - 1);
    tea_set_global(T, "Function");
    tea_push_nil(T);
}

void tea_open_base(tea_State* T)
{
    const tea_CFunction funcs[] = { tea_open_global, tea_open_list, tea_open_map, tea_open_string, tea_open_range, tea_open_buffer, NULL };

    for(int i = 0; funcs[i] != NULL; i++)
    {
        tea_push_cfunction(T, funcs[i], 0);
        tea_call(T, 0);
        tea_pop(T, 1);
    }
}