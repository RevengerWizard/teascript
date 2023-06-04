/*
** tea_fileclass.c
** Teascript file class
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#define tea_fileclass_c
#define TEA_CORE

#include "tea.h"

#include "tea_vm.h"
#include "tea_memory.h"
#include "tea_string.h"
#include "tea_core.h"

static TeaObjectFile* get_file(TeaState* T)
{
    tea_check_file(T, 0);
    TeaObjectFile* file = AS_FILE(T->base[0]);
    if(!file->is_open)
    {
        tea_error(T, "Attempt to use a closed file");
    }
    return file;
}

static void file_closed(TeaState* T)
{
    tea_push_bool(T, !get_file(T)->is_open);
}

static void file_path(TeaState* T)
{
    tea_vm_push(T, OBJECT_VAL(get_file(T)->path));
}

static void file_type(TeaState* T)
{
    tea_vm_push(T, OBJECT_VAL(get_file(T)->type));
}

static void file_write(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaObjectFile* file = get_file(T);

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

static void file_writeline(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    TeaObjectFile* file = get_file(T);

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

static void file_read(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    
    TeaObjectFile* file = get_file(T);

    if(strcmp(file->type->chars, "w") == 0)
    {
        tea_error(T, "File is not readable");
    }

    size_t current_size = BUFFER_SIZE;
    char* contents = TEA_ALLOCATE(T, char, current_size);
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
            contents = TEA_GROW_ARRAY(T, char, contents, old_size, current_size);
        }
    } 
    while(read_bytes == BUFFER_SIZE);

    contents[total_read_bytes] = '\0';

    contents = TEA_GROW_ARRAY(T, char, contents, current_size, total_read_bytes + 1);

    tea_vm_push(T, OBJECT_VAL(tea_string_take(T, contents, total_read_bytes)));
}

static void file_readline(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    TeaObjectFile* file = get_file(T);

    if(strcmp(file->type->chars, "w") == 0)
    {
        tea_error(T, "File is not readable");
    }

    int current_size = BUFFER_SIZE;
    char* line = TEA_ALLOCATE(T, char, current_size);

    while(fgets(line, BUFFER_SIZE, file->file) != NULL) 
    {
        int line_length = strlen(line);
        
        if(line_length == BUFFER_SIZE && line[line_length - 1] != '\n')
        {
            int old_size = current_size;
            current_size += BUFFER_SIZE;
            line = TEA_GROW_ARRAY(T, char, line, old_size, current_size);
        }
        else
        {
            // Remove newline char
            if(line[line_length - 1] == '\n') 
            {
                line_length--;
            }
            line[line_length] = '\0';
            line = TEA_GROW_ARRAY(T, char, line, current_size, line_length + 1);

            tea_vm_push(T, OBJECT_VAL(tea_string_take(T, line, line_length)));
            return;
        }
    }

    TEA_FREE_ARRAY(T, char, line, current_size);
    tea_push_null(T);
}

static void file_seek(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 3, "Expected 1 or 2 arguments, got %d", count);

    TeaObjectFile* file = get_file(T);

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

static void file_close(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    TeaObjectFile* file = get_file(T);

    if(file->is_open == -1)
    {
        tea_error(T, "Cannot close standard file");
    }

    fclose(file->file);
    file->is_open = false;
    
    tea_push_null(T);
}

static void file_iterate(TeaState* T)
{
    file_readline(T);
}

static void file_iteratorvalue(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count != 2, "Expected 2 arguments, got %d", count);
}

static const TeaClass file_class[] = {
    { "closed", "property", file_closed },
    { "path", "property", file_path },
    { "type", "property", file_type },
    { "write", "method", file_write },
    { "writeline", "method", file_writeline },
    { "read", "method", file_read },
    { "readline", "method", file_readline },
    { "seek", "method", file_seek },
    { "close", "method", file_close },
    { "iterate", "method", file_iterate },
    { "iteratorvalue", "method", file_iteratorvalue },
    { NULL, NULL, NULL }
};

void tea_open_file(TeaState* T)
{
    tea_create_class(T, TEA_FILE_CLASS, file_class);
    T->file_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_FILE_CLASS);
    tea_push_null(T);
}