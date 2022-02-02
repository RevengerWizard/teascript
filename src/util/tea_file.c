#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vm/tea_vm.h"
#include "util/tea_file.h"

char* tea_read_file(const char* path)
{
    FILE* file = fopen(path, "rb");
    if(file == NULL)
    {
        fprintf(stderr, "Could not open file \"%s\".\n", path);
        exit(74);
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = (char*)malloc(file_size + 1);
    if(buffer == NULL)
    {
        fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
        exit(74);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    if(bytes_read < file_size)
    {
        fprintf(stderr, "Could not read file \"%s\".\n", path);
        exit(74);
    }

    buffer[bytes_read] = '\0';

    fclose(file);

    return buffer;
}

bool tea_ends_with(const char* name, const char* extension, size_t len)
{
    const char* ldot = strrchr(name, '.');
    if(ldot != NULL)
    {
        if(len == 0)
        {
            len = strlen(extension);
        }
        return strncmp(ldot + 1, extension, len) == 0;
    }

    return false;
}

TeaObjectString* tea_get_directory(char* source)
{
    // find only files with .tea extension
    int len = strlen(source);
    if(tea_ends_with(source, "tea", len)) 
    {
        source = "";
    }

    char res[PATH_MAX];
    if(!tea_resolve_path(".", source, res)) 
    {
        tea_runtime_error("Unable to resolve path '%s'", source);
        exit(1);
    }

    return tea_dir_name(res, strlen(res));
}

TeaObjectString* tea_dir_name(char* path, int len)
{
    if(!len)
    {
        return tea_copy_string(".", 1);
    }

    char* sep = path + len;

    // Trailing slashes
    while(sep != path)
    {
        if(0 == IS_DIR_SEPARATOR(*sep))
        {
            break;
        }
        sep--;
    }

    // First found
    while(sep != path)
    {
        if(IS_DIR_SEPARATOR(*sep))
        {
            break;
        }
        sep--;
    }

    // Trim again
    while(sep != path)
    {
        if(0 == IS_DIR_SEPARATOR(*sep))
        {
            break;
        }
        sep--;
    }

    if(sep == path && !IS_DIR_SEPARATOR(*sep))
    {
        return tea_copy_string(".", 1);
    }

    len = sep - path + 1;

    return tea_copy_string(path, len);
}

bool tea_resolve_path(char* directory, char* path, char* ret)
{
    char buffer[PATH_MAX];
    if(*path == DIR_SEPARATOR)
    {
        snprintf(buffer, PATH_MAX, "%s", path);
    }
    else
    {
        snprintf(buffer, PATH_MAX, "%s%c%s", directory, DIR_SEPARATOR, path);
    }

#ifdef _WIN32
    _fullpath(ret, buffer, PATH_MAX);
#else
    if(realpath(buffer, ret) == NULL)
    {
        return false;
    }
#endif

    return true;
}