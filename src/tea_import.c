/*
** tea_import.c
** Teascript import loading
*/

#define tea_import_c
#define TEA_CORE

#include "tea.h"
#include "tealib.h"

#include "tea_state.h"
#include "tea_import.h"
#include "tea_util.h"
#include "tea_string.h"
#include "tea_do.h"
#include "tea_loadlib.h"

static const TeaReg modules[] = {
    { TEA_MODULE_MATH, tea_import_math },
    { TEA_MODULE_TIME, tea_import_time },
    { TEA_MODULE_OS, tea_import_os },
    { TEA_MODULE_SYS, tea_import_sys },
    { TEA_MODULE_IO, tea_import_io },
    { TEA_MODULE_RANDOM, tea_import_random },
    { NULL, NULL }
};

static void call_native_module(TeaState* T, int index)
{
    tea_push_cfunction(T, modules[index].fn);
    tea_call(T, 0);
}

static int find_native_module(char* name, int length)
{
    for(int i = 0; modules[i].name != NULL; i++) 
    {
        if(strncmp(modules[i].name, name, length) == 0) 
        {
            return i;
        }
    }

    return -1;
}

static bool get_filename_ext(const char* filename, const char* ext) 
{
    const char* dot = strrchr(filename, '.');
    if(!dot || dot == filename) return false;
    return (strcmp(dot, ext) == 0);
}

static bool readable(const char* filename)
{
    FILE* file = fopen(filename, "r");
    if(file)
    {
        fclose(file);
        return true;
    }
    return false;
}

static TeaOString* resolve_filename(TeaState* T, char* dir, char* path_name)
{
    TeaOString* file = NULL;
    size_t l;

    const char* exts[] = { ".tea", /* ".tbc",*/ ".dll", /*".so",*/ "/init.tea" };
    const int n = sizeof(exts) / sizeof(exts[0]);

    for(int i = 0; i < n; i++) 
    {
        l = strlen(path_name) + strlen(exts[i]);
        char* filename = TEA_ALLOCATE(T, char, l + 1);
        sprintf(filename, "%s%s", path_name, exts[i]);

        char path[PATH_MAX];
        if(tea_util_resolve_path(dir, filename, path))
        {
            if(readable(path))
            {
                file = tea_str_copy(T, path, strlen(path));
                TEA_FREE_ARRAY(T, char, filename, l + 1);
                break;
            }
        }

        TEA_FREE_ARRAY(T, char, filename, l + 1);
    }

    return file;
}

void tea_imp_relative(TeaState* T, TeaOString* dir, TeaOString* path_name)
{
    TeaOString* path = resolve_filename(T, dir->chars, path_name->chars);
    if(path == NULL)
    {
        tea_vm_error(T, "Could not resolve path \"%s\"", path_name->chars);
    }

    TeaValue v;
    if(tea_tab_get(&T->modules, path, &v)) 
    {
        T->last_module = AS_MODULE(v);
        tea_vm_push(T, NULL_VAL);
        return;
    }

    char* source = tea_util_read_file(T, path->chars);

    if(source == NULL) 
    {
        tea_vm_error(T, "Could not read \"%s\"", path_name->chars);
    }

    TeaOModule* module = tea_obj_new_module(T, path);
    module->path = tea_util_dirname(T, path->chars, path->length);
    T->last_module = module;

    int status = tea_do_protected_compiler(T, module, source);
    TEA_FREE_ARRAY(T, char, source, strlen(source) + 1);

    if(status != TEA_OK)
        tea_do_throw(T, TEA_SYNTAX_ERROR);

    tea_do_precall(T, T->top[-1], 0);
}

void tea_imp_logical(TeaState* T, TeaOString* name)
{
    TeaValue v;
    if(tea_tab_get(&T->modules, name, &v))
    {
        T->last_module = AS_MODULE(v);
        tea_vm_push(T, v);
        return;
    }

    int index = find_native_module(name->chars, name->length);
    if(index != -1) 
    {
        call_native_module(T, index);
    }
    else
    {
        TeaOString* module = resolve_filename(T, ".", name->chars);
        if(module == NULL)
        {
            tea_vm_error(T, "Unknown module \"%s\"", name->chars);
        }

        printf("dll %s\n", module->chars);
        if(get_filename_ext(module->chars, ".dll"))
        {
            const char* symname = tea_push_fstring(T, TEA_POF "%s", name->chars);

            void* lib = tea_ll_load(T, module->chars);
            TeaCFunction fn = tea_ll_sym(T, lib, symname);
            T->top--;

            tea_push_cfunction(T, fn);
            tea_call(T, 0);
        }
        else
        {
            tea_vm_error(T, "Unknown module \"%s\"", name->chars);
        }
    }

    TeaValue module = T->top[-1];
    T->last_module = AS_MODULE(module);
}