#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_util.h"
#include "tea_vm.h"
#include "tea_memory.h"

char* tea_read_file(TeaState* state, const char* path) 
{
    FILE* file = fopen(path, "rb");
    if(file == NULL) 
    {
        return NULL;
    }

    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);

    char* buffer = ALLOCATE(state, char, file_size + 1);
    if(buffer == NULL) 
    {
        fprintf(stderr, "Not enough memory to read \"%s\"\n", path);
        exit(74);
    }

    size_t bytesRead = fread(buffer, sizeof(char), file_size, file);
    if(bytesRead < file_size) 
    {
        fprintf(stderr, "Could not read file \"%s\"\n", path);
        exit(74);
    }

    buffer[bytesRead] = '\0';

    fclose(file);
    return buffer;
}

TeaObjectString* tea_dirname(TeaState* state, char* path, int len) 
{
    if(!len) 
    {
        return tea_copy_string(state, ".", 1);
    }

    char* sep = path + len;

    /* trailing slashes */
    while(sep != path) 
    {
        if(0 == IS_DIR_SEPARATOR (*sep))
            break;
        sep--;
    }

    /* first found */
    while(sep != path) 
    {
        if(IS_DIR_SEPARATOR (*sep))
            break;
        sep--;
    }

    /* trim again */
    while(sep != path) 
    {
        if(0 == IS_DIR_SEPARATOR (*sep))
            break;
        sep--;
    }

    if(sep == path && !IS_DIR_SEPARATOR(*sep)) 
    {
        return tea_copy_string(state, ".", 1);
    }

    len = sep - path + 1;

    return tea_copy_string(state, path, len);
}

bool tea_resolve_path(char* directory, char* path, char* ret) 
{
    char buf[PATH_MAX];
    if(*path == DIR_SEPARATOR)
    {
        snprintf(buf, PATH_MAX, "%s", path);
    }
    else
    {
        snprintf(buf, PATH_MAX, "%s%c%s", directory, DIR_SEPARATOR, path);
    }

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

bool tea_ends_with(const char* name, const char* extension, size_t length)
{
    const char* ldot = strrchr(name, '.');
    if(ldot != NULL)
    {
        if(length == 0)
            length = strlen(extension);
        return strncmp(ldot + 1, extension, length) == 0;
    }

    return false;
}

TeaObjectString* tea_get_directory(TeaState* state, char* source) 
{
    int len = strlen(source);
    if(!tea_ends_with(source, "tea", len)) 
    {
        source = "";
    }

    char res[PATH_MAX];
    if(!tea_resolve_path(".", source, res)) 
    {
        tea_runtime_error(state->vm, "Unable to resolve path '%s'", source);
        exit(1);
    }

    return tea_dirname(state, res, strlen(res));
}