/*
** tea_iolib.c
** Teascript io module
*/

#include "stdio.h"

#define tea_iolib_c
#define TEA_LIB

#include "tea.h"
#include "tealib.h"

#include "tea_string.h"
#include "tea_import.h"
#include "tea_core.h"
#include "tea_vm.h"

static void create_stdfile(TeaState* T, FILE* f, const char* name, const char* mode)
{
    TeaOFile* file = tea_obj_new_file(T, tea_str_literal(T, ""), tea_str_new(T, mode));
    file->file = f;
    file->is_open = -1;

    tea_vm_push(T, OBJECT_VAL(file));
    tea_set_key(T, 0, name);
}

static const TeaModule io_module[] = {
    { "stdin", NULL },
    { "stdout", NULL },
    { "stderr", NULL },
    { NULL, NULL }
};

TEAMOD_API void tea_import_io(TeaState* T)
{
    tea_create_module(T, TEA_MODULE_IO, io_module);
    create_stdfile(T, stdout, "stdout", "w");
    create_stdfile(T, stdin, "stdin", "r");
    create_stdfile(T, stderr, "stderr", "w");
}