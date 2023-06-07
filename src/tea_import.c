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

void tea_import_native_module(TeaState* T, int index)
{
    tea_push_cfunction(T, modules[index].fn);
    tea_call(T, 0);
}

int tea_find_native_module(char* name, int length)
{
    for(int i = 0; modules[i].name != NULL; i++) 
    {
        if(strncmp(modules[i].name, name, length) == 0 && length == strlen(modules[i].name)) 
        {
            return i;
        }
    }

    return -1;
}

void tea_import_string(TeaState* T, TeaObjectString* mod, TeaObjectString* path_name)
{
    char path[PATH_MAX];
    if(!tea_util_resolve_path(mod->chars, path_name->chars, path))
    {
        tea_vm_runtime_error(T, "Could not open file \"%s\"", path_name->chars);
    }
    TeaObjectString* path_obj = tea_string_new(T, path);

    // If we have imported this file already, skip
    TeaValue module_value;
    if(tea_table_get(&T->modules, path_obj, &module_value)) 
    {
        T->last_module = AS_MODULE(module_value);
        tea_vm_push(T, NULL_VAL);
        return;
    }

    char* source = tea_util_read_file(T, path);

    if(source == NULL) 
    {
        tea_vm_runtime_error(T, "Could not open file \"%s\"", path_name->chars);
    }

    TeaObjectModule* module = tea_obj_new_module(T, path_obj);
    module->path = tea_util_dirname(T, path, strlen(path));
    T->last_module = module;

    int status = teaD_protected_compiler(T, module, source);
    TEA_FREE_ARRAY(T, char, source, strlen(source) + 1);

    if(status != 0)
        tea_do_throw(T, TEA_COMPILE_ERROR);

    teaD_precall(T, T->top[-1], 0);
}

void tea_import_name(TeaState* T, TeaObjectString* name)
{
    // If we have imported this file already, skip
    TeaValue module_val;
    if(tea_table_get(&T->modules, name, &module_val))
    {
        T->last_module = AS_MODULE(module_val);
        tea_vm_push(T, module_val);
        return;
    }

    int index = tea_find_native_module(name->chars, name->length);
    if(index == -1) 
    {
        tea_vm_runtime_error(T, "Unknown module");
    }

    tea_import_native_module(T, index);
    TeaValue module = T->top[-1];
    //printf("::: MOD %s\n", tea_value_type(module));
    T->last_module = AS_MODULE(module);
}