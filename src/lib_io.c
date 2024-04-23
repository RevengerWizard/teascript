/*
** lib_io.c
** Teascript io module
*/

#include <stdio.h>

#define lib_io_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_str.h"
#include "tea_import.h"
#include "tea_vm.h"

static void io_stdfile(tea_State* T, FILE* f, const char* name, const char* mode)
{
    GCfile* file = tea_obj_new_file(T, tea_str_newlit(T, ""), tea_str_new(T, mode));
    file->file = f;
    file->is_open = -1;
    setfileV(T, T->top++, file);
    tea_set_attr(T, 0, name);
}

static const tea_Module io_module[] = {
    { "stdin", NULL },
    { "stdout", NULL },
    { "stderr", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_io(tea_State* T)
{
    tea_create_module(T, TEA_MODULE_IO, io_module);
    io_stdfile(T, stdout, "stdout", "w");
    io_stdfile(T, stdin, "stdin", "r");
    io_stdfile(T, stderr, "stderr", "w");
}