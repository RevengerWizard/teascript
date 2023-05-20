// tea_os.c
// Teascript os module

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea.h"

#include "tea_import.h"
#include "tea_core.h"

#ifdef _WIN32
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

    int l;
    const char* value = getenv(tea_check_lstring(T, 0, &l));
    if(count == 2)
    {
        const char* s = tea_check_string(T, 1);

        if(value != NULL) 
        {
            tea_push_lstring(T, value, l);
            return;
        }

        return;
    }

    if(value != NULL) 
    {
        tea_push_lstring(T, value, l);
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

static void os_system(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    const char* arg = tea_check_string(T, 0);
    
    tea_push_number(T, system(arg));
}

static inline const char* os_name()
{
#if defined(_WIN32) || defined(_WIN64)
    return "windows";
#elif defined(__unix) || defined(__unix__)
    return "unix";
#elif defined(__APPLE__) || defined(__MACH__)
    return "macOS";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freeBSD";
#else
    return "other";
#endif
}

static void init_env(TeaState* T)
{
    // This is not a portable feature on all the C compilers
    extern char** environ;

    tea_new_list(T);

    for(char** current = environ; *current; current++)
    {
        tea_push_string(T, *current);
        tea_add_item(T, 1);
    }

    tea_set_key(T, 0, "env");
}

static const TeaModule os_module[] = {
    { "getenv", os_getenv },
    { "setenv", os_setenv },
    { "system", os_system },
    { "name", NULL },
    { "env", NULL },
    { NULL, NULL }
};

void tea_import_os(TeaState* T)
{
    tea_create_module(T, TEA_OS_MODULE, os_module);    
    tea_push_string(T, os_name());
    tea_set_key(T, 0, "name");
    init_env(T);
}