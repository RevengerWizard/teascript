/*
** tea_util.c
** Teascript utility functions
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define tea_util_c
#define TEA_CORE

#include "tea_util.h"
#include "tea_string.h"
#include "tea_state.h"
#include "tea_memory.h"

char* tea_util_read_file(TeaState* T, const char* path) 
{
    FILE* file = fopen(path, "rb");
    if(file == NULL) 
    {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = TEA_ALLOCATE(T, char, file_size + 1);

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if(bytes_read < file_size) 
    {
        TEA_FREE_ARRAY(T, char, buffer, file_size + 1);
        tea_vm_error(T, "Could not read file \"%s\"", path);
    }

    buffer[bytes_read] = '\0';

    fclose(file);
    return buffer;
}

TeaOString* tea_util_dirname(TeaState* T, char* path, int len) 
{
    if(!len) 
    {
        return tea_str_literal(T, ".");
    }

    char* sep = path + len;

    /* trailing slashes */
    while(sep != path) 
    {
        if(0 == IS_DIR_SEPARATOR(*sep))
            break;
        sep--;
    }

    /* first found */
    while(sep != path) 
    {
        if(IS_DIR_SEPARATOR(*sep))
            break;
        sep--;
    }

    /* trim again */
    while(sep != path) 
    {
        if(0 == IS_DIR_SEPARATOR(*sep))
            break;
        sep--;
    }

    if(sep == path && !IS_DIR_SEPARATOR(*sep)) 
    {
        return tea_str_literal(T, ".");
    }

    len = sep - path + 1;

    return tea_str_copy(T, path, len);
}

bool tea_util_resolve_path(char* directory, char* path, char* ret) 
{
    char buf[PATH_MAX];

    snprintf(buf, PATH_MAX, "%s%c%s", directory, DIR_SEPARATOR, path);

#ifdef _WIN32
    _fullpath(ret, buf, PATH_MAX);
#else
    if(realpath(buf, ret) == NULL) 
    {
        return false;
    }
#endif

    return true;
}

TeaOString* tea_util_get_directory(TeaState* T, char* source) 
{
    char res[PATH_MAX];
    if(!tea_util_resolve_path(".", source, res)) 
    {
        tea_vm_error(T, "Unable to resolve path '%s'", source);
    }

    return tea_util_dirname(T, res, strlen(res));
}