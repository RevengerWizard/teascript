#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "tea_module.h"
#include "tea_core.h"
#include "tea_util.h"

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <dirent.h>
#endif

#if defined(_WIN32) && !defined(S_ISDIR)
#define S_ISDIR(M) (((M) & _S_IFDIR) == _S_IFDIR)
#endif

#ifdef HAS_REALPATH
static TeaValue realpath_path(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(realpath, 1);

    if(!IS_STRING(args[0]))
    {
        NATIVE_ERROR("realpath() takes a string as argument.");
    }

    char* path = AS_CSTRING(args[0]);

    char temp[PATH_MAX + 1]
    if(NULL == realpath(path, temp))
    {
        return NULL_VAL
    }

    return OBJECT_VAL(tea_copy_string(vm->state, temp, strlen(temp)));
}
#endif

static TeaValue isabs_path(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(isabs, 1);

    if(!IS_STRING(args[0]))
    {
        NATIVE_ERROR("isabs() takes a string as argument.");
    }

    char* path = AS_CSTRING(args[0]);

    return (IS_DIR_SEPARATOR(path[0]) ? TRUE_VAL : FALSE_VAL);
}

static TeaValue isdir_path(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(isdir, 1);

    if(!IS_STRING(args[0]))
    {
        NATIVE_ERROR("isdir() takes a string as argument.");
    }

    char* path = AS_CSTRING(args[0]);
    struct stat path_stat;
    stat(path, &path_stat);

    return BOOL_VAL(S_ISDIR(path_stat.st_mode));
}

static TeaValue isfile_path(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(isfile, 1);

    if(!IS_STRING(args[0]))
    {
        NATIVE_ERROR("isfile() takes a string as argument.");
    }

    char* path = AS_CSTRING(args[0]);
    struct stat path_stat;
    stat(path, &path_stat);

    return BOOL_VAL(S_ISREG(path_stat.st_mode));
}

TeaValue tea_import_path(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_PATH_MODULE, 4);
    TeaObjectModule* module = tea_new_module(vm->state, name);

#ifdef HAS_REALPATH
    tea_native_function(vm, &module->values, "realpath", realpath_path);
#endif

    tea_native_function(vm, &module->values, "isabs", isabs_path);
    tea_native_function(vm, &module->values, "isdir", isdir_path);
    tea_native_function(vm, &module->values, "isfile", isfile_path);

    return OBJECT_VAL(module);
}