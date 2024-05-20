/*
** lib_os.c
** Teascript os module
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lib_os_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_arch.h"
#include "tea_import.h"

#if TEA_TARGET_POSIX
#include <sys/types.h> 
#include <sys/stat.h> 
#endif

#if TEA_TARGET_WINDOWS
#include <direct.h>

#define unsetenv(NAME) _putenv_s(NAME, "")
int setenv(const char* name, const char* value, int overwrite)
{
    if(!overwrite && getenv(name) == NULL)
    {
        return 0;
    }

    if(_putenv_s(name, value))
    {
        return -1;
    }
    else
    {
        return 0;
    }
}

#undef mkdir
#define mkdir(dir, mode) _mkdir(dir)
#endif

static void os_getenv(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 1 or 2 arguments, got %d", count);

    const char* value = getenv(tea_check_string(T, 0));
    if(count == 2)
    {
        tea_check_string(T, 1);

        if(value != NULL)
        {
            tea_push_string(T, value);
            return;
        }

        return;
    }

    if(value != NULL)
    {
        tea_push_string(T, value);
        return;
    }

    tea_push_nil(T);
}

static void os_setenv(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1, "Expected at 1 or 2 arguments, got %d", count);

    if(!tea_is_string(T, 0) || (!tea_is_string(T, 1) && !tea_is_nil(T, 1)))
    {
        tea_error(T, "Expected string or nil");
    }

    const char* key = tea_check_string(T, 0);
    int ret;
    if(tea_is_nil(T, 1))
    {
        ret = unsetenv(key);
    }
    else
    {
        ret = setenv(key, tea_check_string(T, 1), 1);
    }

    if(ret == -1)
    {
        tea_error(T, "Failed to set environment variable");
    }

    tea_push_nil(T);
}

static void os_execute(tea_State* T)
{
    const char* arg = tea_check_string(T, 0);
    tea_push_number(T, system(arg));
}

static void os_rename(tea_State* T)
{
    const char* fromname = tea_check_string(T, 0);
    const char* toname = tea_check_string(T, 1);
    tea_push_bool(T, rename(fromname, toname) == 0);
}

static void os_remove(tea_State* T)
{
    const char* filename = tea_check_string(T, 0);
    tea_push_bool(T, remove(filename) == 0);
}

static void os_mkdir(tea_State* T)
{
    const char* dir = tea_check_string(T, 0);
    if(mkdir(dir, 0777) == -1)
    {
        tea_error(T, "Cannot create directory");
    }
}

/* ------------------------------------------------------------------------ */

static void init_env(tea_State* T)
{
    extern char** environ;

    tea_new_map(T);

    for(char** current = environ; *current; current++)
    {
        const char* key = tea_push_string(T, *current);
        char* value = strchr(key, '=');
        if(value != NULL)
        {
            *value = '\0';  /* Split the variable and value */
            value++;
            tea_push_string(T, value);
        }
        else
        {
            tea_push_nil(T);
        }
        tea_set_field(T, -3);
    }

    tea_set_attr(T, 0, "env");
}

static const tea_Module os_module[] = {
    { "getenv", os_getenv, TEA_VARARGS },
    { "setenv", os_setenv, TEA_VARARGS },
    { "execute", os_execute, 1 },
    { "remove", os_remove, 1 },
    { "rename", os_rename, 2 },
    { "mkdir", os_mkdir, 1 },
    { "name", NULL },
    { "arch", NULL },
    { "env", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_os(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_OS, os_module);
    tea_push_string(T, TEA_OS_NAME);
    tea_set_attr(T, 0, "name");
    tea_push_string(T, TEA_ARCH_NAME);
    tea_set_attr(T, 0, "arch");
    init_env(T);
}