/*
** lib_core.c
** Teascript core classes and functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define lib_core_c
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

static void core_print(tea_State* T)
{
    int count = tea_get_top(T);
    if(count == 0)
    {
        putchar('\n');
        tea_push_null(T);
        return;
    }

    for(int i = 0; i < count; i++)
    {
        int len;
        const char* string = tea_to_lstring(T, i, &len);
        if(i)
            putchar('\t');

        fwrite(string, sizeof(char), len, stdout);
        tea_pop(T, 1);
    }

    putchar('\n');
    tea_push_null(T);
}

static void core_input(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count > 1, "Expected 0 or 1 arguments, got %d", count);
    if(count != 0)
    {
        int len;
        const char* prompt = tea_check_lstring(T, 0, &len);
        fwrite(prompt, sizeof(char), len, stdout);
    }

    uint64_t current_size = 128;
    char* line = tea_mem_newvec(T, char, current_size);

    int c = EOF;
    uint64_t len = 0;
    while((c = getchar()) != '\n' && c != EOF)
    {
        line[len++] = (char)c;

        if(len + 1 == current_size)
        {
            int old_size = current_size;
            current_size = TEA_MEM_GROW(current_size);
            line = tea_mem_reallocvec(T, char, line, old_size, current_size);
        }
    }

    /* If length has changed, shrink */
    if(len != current_size)
    {
        line = tea_mem_reallocvec(T, char, line, current_size, len + 1);
    }

    line[len] = '\0';

    GCstr* s = tea_str_take(T, line, len);
    setstrV(T, T->top++, s);
}

static void core_open(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 1 or 2 arguments, got %d", count);

    const char* path = tea_check_string(T, 0);
    const char* type = "r";

    if(count == 2)
        type = tea_check_string(T, 1);

    FILE* fp = fopen(path, type);
    if(fp == NULL)
    {
        tea_error(T, "Unable to open file " TEA_QS, path);
    }

    GCfile* file = tea_obj_new_file(T, tea_str_new(T, path), tea_str_new(T, type));
    file->file = fp;
    setfileV(T, T->top++, file);
}

static void core_assert(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1, "Expected at least 1 argument, got %d", count);
    if(!tea_to_bool(T, 0))
    {
        tea_error(T, "%s", tea_opt_string(T, 1, "assertion failed!"));
    }
    tea_push_value(T, 0);
}

static void core_error(tea_State* T)
{
    tea_error(T, "%s", tea_check_string(T, 0));
    tea_push_null(T);
}

static void core_typeof(tea_State* T)
{
    tea_push_string(T, tea_typeof(T, 0));
}

static void core_gc(tea_State* T)
{
    int collected = tea_gc(T);
    tea_push_number(T, collected);
}

static void core_eval(tea_State* T)
{
    int len;
    const char* s = tea_check_lstring(T, 0, &len);
    if(tea_load_buffer(T, s, len, "?<interpret>") == TEA_OK)
    {
        tea_call(T, 0);
    }
    tea_push_null(T);
}

static int writer_buf(tea_State* T, void* sb, const void* p, size_t size)
{
    tea_buf_putmem(T, (SBuf*)sb, p, size);
    UNUSED(T);
    return 0;
}

static void core_dump(tea_State* T)
{
    tea_check_function(T, 0);
    GCfunc* fn = funcV(T->base);
    SBuf* sb = tea_buf_tmp_(T);
    if(!isteafunc(fn) || tea_bcwrite(T, fn->t.proto, writer_buf, sb))
    {
        tea_error(T, "Unable to dump given function");
    }
    GCstr* str = tea_buf_str(T, sb);
    setstrV(T, T->top++, str);
}

static void core_loadfile(tea_State* T)
{
    const char* fname = tea_check_string(T, 0);
    const char* mode = tea_opt_string(T, 1, "t");
    int status = tea_load_filex(T, fname, NULL, mode);
    if(status != TEA_OK)
    {
        tea_error(T, "Unable to load file");
    }
}

static void core_loadstring(tea_State* T)
{
    int len;
    const char* str = tea_check_lstring(T, 0, &len);
    const char* name = tea_opt_string(T, 1, "t");
    int status = tea_load_bufferx(T, str, len, name ? name : "?<load>", NULL);
    if(status != TEA_OK)
    {
        tea_error(T, "Unable to load string");
    }
}

static void core_char(tea_State* T)
{
    int n = tea_check_number(T, 0);
    GCstr* c = tea_utf_from_codepoint(T, n);
    setstrV(T, T->top++, c);
}

static void core_ord(tea_State* T)
{
    const char* c = tea_check_string(T, 0);
    tea_push_number(T, tea_utf_decode((uint8_t*)c, 1));
}

static void core_hex(tea_State* T)
{
    double n = tea_check_number(T, 0);
    tea_strfmt_pushf(T, "0x%x", (unsigned int)n);
}

static void core_bin(tea_State* T)
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

static void core_rawequal(tea_State* T)
{
    cTValue* o1 = tea_lib_checkany(T, 0);
    cTValue* o2 = tea_lib_checkany(T, 1);
    tea_push_bool(T, tea_obj_rawequal(o1, o2));
}

static void core_hasattr(tea_State* T)
{
    tea_check_any(T, 0);
    const char* attr = tea_check_string(T, 1);
    bool found = tea_get_attr(T, 0, attr);
    tea_push_bool(T, found);
}

static void core_getattr(tea_State* T)
{
    tea_check_any(T, 0);
    const char* attr = tea_check_string(T, 1);
    bool found = tea_get_attr(T, 0, attr);
    if(!found)
        tea_error(T, TEA_QS " has no attribute " TEA_QS, tea_typename(T->base), attr);
}

static void core_setattr(tea_State* T)
{
    tea_check_any(T, 0);
    const char* attr = tea_check_string(T, 1);
    tea_check_any(T, 2);
    tea_set_attr(T, 0, attr);
    T->top--;
}

static void bool_constructor(tea_State* T)
{
    bool b = tea_to_bool(T, 1);
    tea_push_bool(T, b);
}

static void number_constructor(tea_State* T)
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
    tea_push_null(T);
}

static const tea_Class number_class[] = {
    { "constructor", "method", number_constructor, TEA_VARARGS },
    { NULL, NULL }
};

static const tea_Class bool_class[] = {
    { "constructor", "method", bool_constructor, 2 },
    { NULL, NULL }
};

static const tea_Reg globals[] = {
    { "print", core_print, TEA_VARARGS },
    { "input", core_input, TEA_VARARGS },
    { "open", core_open, TEA_VARARGS },
    { "assert", core_assert, TEA_VARARGS },
    { "error", core_error, 1 },
    { "typeof", core_typeof, 1 },
    { "gc", core_gc, 0 },
    { "eval", core_eval, 1 },
    { "dump", core_dump, 1 },
    { "loadfile", core_loadfile, 1 },
    { "loadstring", core_loadstring, 1 },
    { "char", core_char, 1 },
    { "ord", core_ord, 1 },
    { "hex", core_hex, 1 },
    { "bin", core_bin, 1 },
    { "rawequal", core_rawequal, 2 },
    { "hasattr", core_hasattr, 2 },
    { "getattr", core_getattr, 2 },
    { "setattr", core_setattr, 3 },
    { NULL, NULL }
};

static void tea_open_global(tea_State* T)
{
    tea_set_funcs(T, globals);
    tea_create_class(T, "Number", number_class);
    T->number_class = classV(T->top - 1);
    tea_set_global(T, "Number");
    tea_create_class(T, "Bool", bool_class);
    T->bool_class = classV(T->top - 1);
    tea_set_global(T, "Bool");
    tea_push_null(T);
}

void tea_open_core(tea_State* T)
{
    const tea_CFunction core[] = { tea_open_global, tea_open_file, tea_open_list, tea_open_map, tea_open_string, tea_open_range, NULL };

    for(int i = 0; core[i] != NULL; i++)
    {
        tea_push_cfunction(T, core[i], 0);
        tea_call(T, 0);
        tea_pop(T, 1);
    }
}