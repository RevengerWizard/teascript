/*
** lib_sys.c
** Teascript sys module
*/

#include <stdlib.h>
#include <math.h>
#include <time.h>

#define lib_sys_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_arch.h"
#include "tea_import.h"

static void sys_exit(tea_State* T)
{
    int status;
    if(tea_is_bool(T, 0))
    {
        status = (tea_to_bool(T, 0) ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    else
    {
        status = (int)tea_opt_integer(T, 0, EXIT_SUCCESS);
    }
    tea_close(T);
    exit(status);
}

/* ------------------------------------------------------------------------ */

static void init_argv(tea_State* T)
{
    int argc = T->argc;
    char** argv = T->argv;
    int argf = T->argf;
    tea_new_list(T);
    for(int i = 0; i < argc - argf; i++)
    {
        tea_push_string(T, argv[i + argf]);
        tea_add_item(T, 1);
    }
    tea_set_attr(T, 0, "argv");
}

static void set_version(tea_State* T)
{
    tea_new_map(T);
    tea_push_number(T, TEA_VERSION_MAJOR);
    tea_set_key(T, 1, "major");
    tea_push_number(T, TEA_VERSION_MINOR);
    tea_set_key(T, 1, "minor");
    tea_push_number(T, TEA_VERSION_PATCH);
    tea_set_key(T, 1, "patch");
    tea_set_attr(T, 0, "version");
}

static const tea_Reg sys_module[] = {
    { "exit", sys_exit, -1 },
    { "argv", NULL },
    { "version", NULL },
    { "byteorder", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_sys(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_SYS, sys_module);
    tea_push_string(T, TEA_ARCH_BYTEORDER);
    tea_set_attr(T, 0, "byteorder");
    init_argv(T);
    set_version(T);
}