/*
** lib_file.c
** Teascript file class
*/

#define lib_file_c
#define TEA_CORE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#include "tea.h"
#include "tealib.h"

#include "tea_vm.h"
#include "tea_str.h"
#include "tea_gc.h"

static GCfile* get_file(tea_State* T)
{
    tea_check_file(T, 0);
    GCfile* file = AS_FILE(T->base[0]);
    if(!file->is_open)
    {
        tea_error(T, "Attempt to use a closed file");
    }
    return file;
}

static void file_closed(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_push_bool(T, !get_file(T)->is_open);
}

static void file_path(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_vm_push(T, OBJECT_VAL(get_file(T)->path));
}

static void file_type(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_vm_push(T, OBJECT_VAL(get_file(T)->type));
}

static void file_write(tea_State* T)
{
    GCfile* file = get_file(T);

    int len;
    const char* string = tea_check_lstring(T, 1, &len);

    if(strcmp(file->type->chars, "r") == 0)
    {
        tea_error(T, "File is not writable");
    }

    int chars_wrote = fwrite(string, sizeof(char), len, file->file);
    fflush(file->file);

    tea_push_number(T, chars_wrote);
}

static void file_writeline(tea_State* T)
{
    GCfile* file = get_file(T);

    int len;
    const char* string = tea_check_lstring(T, 1, &len);

    if(strcmp(file->type->chars, "r") == 0)
    {
        tea_error(T, "File is not writable");
    }

    int chars_wrote = fwrite(string, sizeof(char), len, file->file);
    chars_wrote += fwrite("\n", sizeof(char), 1, file->file);
    fflush(file->file);

    tea_push_number(T, chars_wrote);
}

#define BUFFER_SIZE 1024

static void file_read(tea_State* T)
{
    GCfile* file = get_file(T);

    if(strcmp(file->type->chars, "w") == 0)
    {
        tea_error(T, "File is not readable");
    }

    size_t current_size = BUFFER_SIZE;
    char* contents = tea_mem_new(T, char, current_size);
    size_t read_bytes = 0;
    size_t total_read_bytes = 0;
    do
    {
        read_bytes = fread(contents + total_read_bytes, sizeof(char), BUFFER_SIZE, file->file);
        total_read_bytes += read_bytes;
        if(total_read_bytes + BUFFER_SIZE > current_size)
        {
            int old_size = current_size;
            current_size += BUFFER_SIZE;
            contents = tea_mem_reallocvec(T, char, contents, old_size, current_size);
        }
    }
    while(read_bytes == BUFFER_SIZE);

    contents[total_read_bytes] = '\0';

    contents = tea_mem_reallocvec(T, char, contents, current_size, total_read_bytes + 1);

    tea_vm_push(T, OBJECT_VAL(tea_str_take(T, contents, total_read_bytes)));
}

static void file_readline(tea_State* T)
{
    GCfile* file = get_file(T);

    if(strcmp(file->type->chars, "w") == 0)
    {
        tea_error(T, "File is not readable");
    }

    int current_size = BUFFER_SIZE;
    char* line = tea_mem_new(T, char, current_size);

    while(fgets(line, BUFFER_SIZE, file->file) != NULL)
    {
        int line_len = strlen(line);

        if(line_len == BUFFER_SIZE && line[line_len - 1] != '\n')
        {
            int old_size = current_size;
            current_size += BUFFER_SIZE;
            line = tea_mem_reallocvec(T, char, line, old_size, current_size);
        }
        else
        {
            /* Remove newline char */
            if(line[line_len - 1] == '\n')
            {
                line_len--;
            }
            line[line_len] = '\0';
            line = tea_mem_reallocvec(T, char, line, current_size, line_len + 1);

            tea_vm_push(T, OBJECT_VAL(tea_str_take(T, line, line_len)));
            return;
        }
    }

    tea_mem_freevec(T, char, line, current_size);
    tea_push_null(T);
}

static void file_seek(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 3, "Expected 1 or 2 arguments, got %d", count);

    GCfile* file = get_file(T);

    int seek_type = SEEK_SET;
    if(count == 3)
    {
        int seek_type_num = tea_check_number(T, 2);
        switch(seek_type_num)
        {
            case 0:
                seek_type = SEEK_SET;
                break;
            case 1:
                seek_type = SEEK_CUR;
                break;
            case 2:
                seek_type = SEEK_END;
                break;
            default:
                seek_type = SEEK_SET;
                break;
        }
    }

    int offset = tea_check_number(T, 1);

    if(offset != 0 && !strstr(file->type->chars, "b"))
    {
        tea_error(T, "May not have non-zero offset if file is opened in text mode");
    }

    if(fseek(file->file, offset, seek_type))
    {
        tea_error(T, "Unable to seek file");
    }

    tea_push_null(T);
}

static void file_close(tea_State* T)
{
    GCfile* file = get_file(T);

    if(file->is_open == -1)
    {
        tea_error(T, "Cannot close standard file");
    }

    fclose(file->file);
    file->is_open = false;

    tea_push_null(T);
}

static void file_iterate(tea_State* T)
{
    file_readline(T);
}

static void file_iteratorvalue(tea_State* T) {}

static const tea_Class file_class[] = {
    { "closed", "property", file_closed, TEA_VARARGS },
    { "path", "property", file_path, TEA_VARARGS },
    { "type", "property", file_type, TEA_VARARGS },
    { "write", "method", file_write, 2 },
    { "writeline", "method", file_writeline, 2 },
    { "read", "method", file_read, 1 },
    { "readline", "method", file_readline, 1 },
    { "seek", "method", file_seek, TEA_VARARGS },
    { "close", "method", file_close, 1 },
    { "iterate", "method", file_iterate, 2 },
    { "iteratorvalue", "method", file_iteratorvalue, 2 },
    { NULL, NULL, NULL }
};

void tea_open_file(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_FILE, file_class);
    T->file_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_CLASS_FILE);
    tea_push_null(T);
}