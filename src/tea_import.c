/*
** tea_import.c
** Teascript import loading
*/

#define tea_import_c
#define TEA_CORE

#include "tea.h"

#include "tea_state.h"
#include "tea_import.h"
#include "tea_util.h"
#include "tea_string.h"
#include "tea_do.h"

static const TeaReg modules[] = {
    { TEA_MATH_MODULE, tea_import_math },
    { TEA_TIME_MODULE, tea_import_time },
    { TEA_OS_MODULE, tea_import_os },
    { TEA_SYS_MODULE, tea_import_sys },
    { TEA_IO_MODULE, tea_import_io },
    { TEA_RANDOM_MODULE, tea_import_random },
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

static TeaObjectString* resolve_filename(TeaState* T, char* module, char* path_name)
{
    char path[PATH_MAX];
    if(!tea_util_resolve_path(module, path_name, path))
    {
        tea_vm_error(T, "Could not resolve path \"%s\"", path_name);
    }

    char* file = NULL;
    size_t l;

    const char* exts[] = { ".tea" };
    const int n = sizeof(exts) / sizeof(exts[0]);

    for(int i = 0; i < n; i++) 
    {
        l = strlen(path) + strlen(exts[i]);
        char* filename = TEA_ALLOCATE(T, char, l + 1); // path length + extension length + null terminator
        sprintf(filename, "%s%s", path, exts[i]);

        if(readable(filename))
        {
            file = filename;
            break;
        }

        TEA_FREE_ARRAY(T, char, filename, l + 1);
    }

    if(!file)
        tea_vm_error(T, "File \"%s\" not found", path_name);

    return tea_string_take(T, file, l);
}

void tea_import_relative(TeaState* T, TeaObjectString* mod, TeaObjectString* path_name)
{
    TeaObjectString* path = resolve_filename(T, mod->chars, path_name->chars);

    TeaValue v;
    if(tea_table_get(&T->modules, path, &v)) 
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

    TeaObjectModule* module = tea_obj_new_module(T, path);
    module->path = tea_util_dirname(T, path->chars, path->length);
    T->last_module = module;

    int status = teaD_protected_compiler(T, module, source);
    TEA_FREE_ARRAY(T, char, source, strlen(source) + 1);

    if(status != 0)
        tea_do_throw(T, TEA_COMPILE_ERROR);

    teaD_precall(T, T->top[-1], 0);
}

void tea_import_logical(TeaState* T, TeaObjectString* name)
{
    TeaValue v;
    if(tea_table_get(&T->modules, name, &v))
    {
        T->last_module = AS_MODULE(v);
        tea_vm_push(T, v);
        return;
    }

    int index = find_native_module(name->chars, name->length);
    if(index == -1) 
    {
        tea_vm_error(T, "Unknown module \"%s\"", name->chars);
    }

    call_native_module(T, index);
    TeaValue module = T->top[-1];
    T->last_module = AS_MODULE(module);
}