// tea_core.c
// Teascript core classes and functions

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "tea.h"

#include "tea_vm.h"
#include "tea_memory.h"
#include "tea_core.h"
#include "tea_utf.h"

static void print(TeaState* T)
{
    int count = tea_get_top(T);
    if(count == 0)
    {
        tea_write_line();
        tea_push_null(T);
        return;
    }

    for(int i = 0; i < count; i++)
    {
        int len;
        const char* string = tea_to_lstring(T, i, &len);
        tea_write_string(string, len);
        tea_write_string("\t", 1);
        tea_pop(T, 1);
    }

    tea_write_line();
    tea_push_null(T);
}

static void input(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);
    if(count != 0)
    {
        int len;
        const char* prompt = tea_check_lstring(T, 0, &len);
        tea_write_string(prompt, len);
    }

    uint64_t current_size = 128;
    char* line = ALLOCATE(T, char, current_size);

    int c = EOF;
    uint64_t length = 0;
    while((c = getchar()) != '\n' && c != EOF) 
    {
        line[length++] = (char)c;

        if(length + 1 == current_size) 
        {
            int old_size = current_size;
            current_size = GROW_CAPACITY(current_size);
            line = GROW_ARRAY(T, char, line, old_size, current_size);
        }
    }

    // If length has changed, shrink
    if(length != current_size) 
    {
        line = GROW_ARRAY(T, char, line, current_size, length + 1);
    }
    
    line[length] = '\0';

    T->slot[T->top++] = OBJECT_VAL(tea_take_string(T, line, length));
}

static void open(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 1 or 2 arguments, got %d", count);

    const char* path = tea_check_string(T, 0);
    const char* type = "r";

    if(count == 2)
        type = tea_check_string(T, 1);

    TeaObjectFile* file = tea_new_file(T, tea_new_string(T, path), tea_new_string(T, type));
    file->file = fopen(file->path->chars, file->type->chars);

    if(file->file == NULL) 
    {
        tea_error(T, "Unable to open file '%s'", path);
    }
    T->slot[T->top++] = OBJECT_VAL(file);
}

static void assert(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    if(tea_falsey(T, 0))
    {
        tea_error(T, "%s", tea_get_string(T, 1));
    }
    tea_push_value(T, 0);
}

static void error(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_error(T, "%s", tea_get_string(T, 0));
    tea_push_null(T);
}

static void type(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    tea_push_string(T, tea_type_name(T, 0));
}

static void gc(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 0);
    tea_collect_garbage(T);
    tea_push_null(T);
}

static void interpret(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    TeaState* T1 = tea_open();
    tea_interpret(T1, "interpret", tea_check_string(T, 0));
    tea_close(T1);
}

static void chr(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    int n = tea_check_number(T, 0);
    T->slot[T->top++] = OBJECT_VAL(tea_ustring_from_code_point(T, n));
}

static void ord(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    const char* c = tea_check_string(T, 0);
    tea_push_int(T, tea_ustring_decode((uint8_t*)c, 1));
}

static void hex(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    int n = tea_check_int(T, 0);

    int len = snprintf(NULL, 0, "0x%x", n);
    char* string = ALLOCATE(T, char, len + 1);
    snprintf(string, len + 1, "0x%x", n);

    T->slot[T->top++] = OBJECT_VAL(tea_take_string(T, string, len));
}

static void bin(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    int n = tea_check_int(T, 0);

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

    // Reverse the buffer to get the binary representation in the correct order
    char temp;
    for(int j = 0; j < (i - 2) / 2; j++) 
    {
        temp = buffer[j + 2];
        buffer[j + 2] = buffer[i - j - 1];
        buffer[i - j - 1] = temp;
    }

    tea_push_string(T, buffer);
}

static void number(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    int is_num;
    double n = tea_to_numberx(T, 0, &is_num);
    if(!is_num)
    {
        tea_error(T, "Failed conversion");
    }
    tea_push_number(T, n);
}

static const TeaReg globals[] = {
    { "print", print },
    { "input", input },
    { "open", open },
    { "assert", assert },
    { "error", error },
    { "type", type },
    { "gc", gc },
    { "interpret", interpret },
    { "char", chr },
    { "ord", ord },
    { "hex", hex },
    { "bin", bin },
    { "number", number },
    { NULL, NULL }
};

static void tea_open_global(TeaState* T)
{
    tea_set_funcs(T, globals);
}

void tea_open_core(TeaState* T)
{
    const TeaCFunction core[] = { tea_open_global, tea_open_file, tea_open_list, tea_open_map, tea_open_string, tea_open_range, NULL };

    for(int i = 0; core[i] != NULL; i++)
    {
        tea_push_cfunction(T, core[i]);
        tea_call(T, 0);
    }
}