/*
** tea_oslib.c
** Teascript os module
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define tea_oslib_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_arch.h"
#include "tea_import.h"
#include "tea_core.h"

#if TEA_TARGET_WINDOWS
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
#endif

static void os_getenv(TeaState* T)
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

    tea_push_null(T);
}

static void os_setenv(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    if(!tea_is_string(T, 0) || (!tea_is_string(T, 1) && !tea_is_null(T, 1)))
    {
        tea_error(T, "Expected string or null");
    }

    const char* key = tea_get_string(T, 0);
    int ret;
    if(tea_is_null(T, 1))
    {
        ret = unsetenv(key);
    }
    else
    {
        ret = setenv(key, tea_get_string(T, 1), 1);
    }

    if(ret == -1)
    {
        tea_error(T, "Failed to set environment variable");
    }

    tea_push_null(T);
}

static void os_execute(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    const char* arg = tea_check_string(T, 0);

    tea_push_number(T, system(arg));
}

static void init_env(TeaState* T)
{
    /* This is not a portable feature on all the C compilers */
    extern char** environ;

    tea_new_map(T);

    for(char** current = environ; *current; current++)
    {
        char* key = strdup(*current);  /* Create a copy of the environment variable */
        char* value = strchr(key, '=');

        if(value != NULL)
        {
            *value = '\0';  /* Split the variable and value */
            value++;

            tea_push_string(T, value);
            tea_set_key(T, -2, key);
        }

        free(key);
    }

    tea_set_key(T, 0, "env");
}

static const TeaModule os_module[] = {
    { "getenv", os_getenv },
    { "setenv", os_setenv },
    { "execute", os_execute },
    { "name", NULL },
    { "env", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_os(TeaState* T)
{
    tea_create_module(T, TEA_MODULE_OS, os_module);
    tea_push_string(T, TEA_OS_NAME);
    tea_set_key(T, 0, "name");
    init_env(T);
}