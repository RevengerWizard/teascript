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
#include "tea_meta.h"

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
    if(!tea_to_bool(T, 0))
    {
        if(tvisstr(T->base + 1))
            tea_error(T, tea_check_string(T, 1));
        else
            tea_err_msg(T, TEA_ERR_ASSERT);
    }
    tea_push_value(T, 0);
}

static void base_error(tea_State* T)
{
    tea_check_any(T, 0);
    tea_throw(T);
}

static void base_typeof(tea_State* T)
{
    tea_push_string(T, tea_typeof(T, 0));
}

static void base_tonumber(tea_State* T)
{
    int base = tea_opt_number(T, 1, 10);
    if(base == 10)
    {
        bool is_num;
        double n = tea_to_numberx(T, 0, &is_num);
        if(is_num)
        {
            tea_push_number(T, n);
            return;
        }
    }
    else
    {
        const char* p = str_data(tea_lib_checkstr(T, 0));
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

static void base_tostring(tea_State* T)
{
    tea_to_string(T, 0);
}

static void base_gc(tea_State* T)
{
    int collected = tea_gc(T);
    tea_push_number(T, collected);
}

static void base_eval(tea_State* T)
{
    const char* s = tea_check_string(T, 0);
    int status = tea_eval(T, s);
    if(status == TEA_OK)
    {
        tea_call(T, 0);
    }
    else
    {
        /* Rethrow the error */
        tea_err_throw(T, status);
    }
}

static int writer_buf(tea_State* T, void* sb, const void* p, size_t size)
{
    tea_buf_putmem(T, (SBuf*)sb, p, size);
    UNUSED(T);
    return 0;
}

static void base_dump(tea_State* T)
{
    GCproto* pt = tea_lib_checkTproto(T, 0, true);
    uint32_t flags = 0;
    SBuf* sb;
    TValue* o = T->base + 1;
    if(o < T->top)
    {
        if(tvisstr(o))
        {
            const char* mode = strVdata(o);
            char c;
            while((c = *mode++))
            {
                if(c == 's') flags |= BCDUMP_F_STRIP;
            }
        }
        else if(tvisbool(o) && boolV(o))
        {
            flags |= BCDUMP_F_STRIP;
        }
    }
    sb = tea_buf_tmp_(T);
    T->top = T->base + 1;
    if(!pt || tea_bcwrite(T, pt, writer_buf, sb, flags))
    {
        tea_err_msg(T, TEA_ERR_DUMP);
    }
    setstrV(T, T->top - 1, tea_buf_str(T, sb));
}

static void base_loadfile(tea_State* T)
{
    const char* fname = tea_check_string(T, 0);
    const char* mode = tea_opt_string(T, 1, "t");
    int status = tea_load_filex(T, fname, NULL, mode);
    if(status != TEA_OK)
    {
        /* Rethrow the error */
        tea_err_throw(T, status);
    }
}

static void base_loadstring(tea_State* T)
{
    size_t len;
    const char* str = tea_check_lstring(T, 0, &len);
    const char* name = tea_opt_string(T, 1, "?<load>");
    int status = tea_load_bufferx(T, str, len, name, NULL);
    if(status != TEA_OK)
    {
        /* Rethrow the error */
        tea_err_throw(T, status);
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
    tea_push_bool(T, tea_has_attr(T, 0, attr));
}

static void base_getattr(tea_State* T)
{
    TValue* tv = tea_lib_checkany(T, 0);
    GCstr* attr = tea_lib_checkstr(T, 1);
    cTValue* o = tea_meta_getattr(T, attr, tv);
    copyTV(T, T->top++, o);
}

static void base_setattr(tea_State* T)
{
    tea_check_any(T, 0);
    TValue* o = tea_lib_checkany(T, 0);
    GCstr* attr = tea_lib_checkstr(T, 1);
    TValue* item = tea_lib_checkany(T, 2);
    tea_meta_setattr(T, attr, o, item);
    tea_push_nil(T);
}

static void base_delattr(tea_State* T)
{
    TValue* o = tea_lib_checkany(T, 0);
    GCstr* attr = tea_lib_checkstr(T, 1);
    tea_meta_delattr(T, attr, o);
}

static void bool_init(tea_State* T)
{
    bool b = tea_to_bool(T, 1);
    tea_push_bool(T, b);
}

static void number_init(tea_State* T)
{
    tea_remove(T, 0);
    base_tonumber(T);
}

static void invalid_init(tea_State* T)
{
    tea_error(T, "Invalid init");
}

/* ------------------------------------------------------------------------ */

static const tea_Methods func_class[] = {
    { "new", "method", invalid_init, TEA_VARG },
    { NULL, NULL }
};

static const tea_Methods number_class[] = {
    { "new", "method", number_init, 2, 1 },
    { NULL, NULL }
};

static const tea_Methods bool_class[] = {
    { "new", "method", bool_init, 2, 0 },
    { NULL, NULL }
};

static const tea_Reg globals[] = {
    { "print", base_print, TEA_VARG, 0 },
    { "input", base_input, 0, 1 },
    { "assert", base_assert, 1, 1 },
    { "error", base_error, 1, 0 },
    { "typeof", base_typeof, 1, 0 },
    { "tonumber", base_tonumber, 1, 1 },
    { "tostring", base_tostring, 1, 0 },
    { "gc", base_gc, 0, 0 },
    { "eval", base_eval, 1, 0 },
    { "dump", base_dump, 1, 1 },
    { "loadfile", base_loadfile, 1, 0 },
    { "loadstring", base_loadstring, 1, 1 },
    { "char", base_char, 1, 0 },
    { "ord", base_ord, 1, 0 },
    { "hex", base_hex, 1, 0 },
    { "bin", base_bin, 1, 0 },
    { "rawequal", base_rawequal, 2, 0 },
    { "hasattr", base_hasattr, 2, 0 },
    { "getattr", base_getattr, 2, 0 },
    { "setattr", base_setattr, 3, 0 },
    { "delattr", base_delattr, 2, 0 },
    { NULL, NULL }
};

static void tea_open_global(tea_State* T)
{
    const tea_Reg* reg = globals;
    for(; reg->name; reg++)
    {
        tea_push_cfunction(T, reg->fn, reg->nargs, reg->nopts);
        tea_set_global(T, reg->name);
    }

    T->object_class = tea_class_new(T, tea_str_newlit(T, "Object"));
    setclassV(T, T->top++, T->object_class);
    tea_set_global(T, "Object");

    tea_create_class(T, "Number", number_class);
    T->number_class = classV(T->top - 1);
    T->number_class->super = NULL;
    tea_set_global(T, "Number");
    tea_create_class(T, "Bool", bool_class);
    T->bool_class = classV(T->top - 1);
    T->bool_class->super = NULL;
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
        tea_push_cfunction(T, funcs[i], 0, 0);
        tea_call(T, 0);
        tea_pop(T, 1);
    }
}