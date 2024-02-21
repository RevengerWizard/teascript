/*
** tea_import.c
** Import loader
*/

#define tea_import_c
#define TEA_CORE

#include <stdlib.h>
#include <string.h>

#include "tea.h"
#include "tealib.h"

#include "tea_arch.h"
#include "tea_state.h"
#include "tea_vm.h"
#include "tea_import.h"
#include "tea_str.h"
#include "tea_err.h"
#include "tea_tab.h"

#if TEA_TARGET_DLOPEN

#include <dlfcn.h>

static void ll_unload(void* lib)
{
    dlclose(lib);
}

static void* ll_load(tea_State* T, const char* path)
{
    void* lib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if(lib == NULL)
        tea_error(T, dlerror());
    return lib;
}

static tea_CFunction ll_sym(tea_State* T, void* lib, const char* sym)
{
    tea_CFunction f = (tea_CFunction)dlsym(lib, sym);
    if(f == NULL)
        tea_error(T, dlerror());
    return f;
}

#elif TEA_TARGET_WINDOWS

#include <windows.h>

static void ll_error(tea_State* T)
{
    int error = GetLastError();
    char buffer[128];
    if(FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, buffer, sizeof(buffer) / sizeof(char), NULL))
        tea_error(T, buffer);
    else
        tea_error(T, "system error %d\n", error);
}

static void ll_unload(void* lib)
{
    FreeLibrary((HMODULE)lib);
}

static void* ll_load(tea_State* T, const char* path)
{
    HMODULE lib = LoadLibraryExA(path, NULL, 0);
    if(lib == NULL)
        ll_error(T);
    return lib;
}

static tea_CFunction ll_sym(tea_State* T, void* lib, const char* sym)
{
    tea_CFunction f = (tea_CFunction)GetProcAddress((HMODULE)lib, sym);
    if(f == NULL)
        ll_error(T);
    return f;
}

#else

#define DLMSG   "dynamic libraries not enabled"

static void ll_unload(void* lib)
{
    /* Not used */
    UNUSED(lib);
}

static void* ll_load(tea_State* T, const char* path)
{
    UNUSED(path);
    tea_error(T, DLMSG);
    return NULL;
}

static tea_CFunction ll_sym(tea_State* T, void* lib, const char* sym)
{
    UNUSED(lib); UNUSED(sym);
    tea_error(T, DLMSG);
    return NULL;
}

#endif

static const tea_Reg modules[] = {
    { TEA_MODULE_MATH, tea_import_math },
    { TEA_MODULE_TIME, tea_import_time },
    { TEA_MODULE_OS, tea_import_os },
    { TEA_MODULE_SYS, tea_import_sys },
    { TEA_MODULE_IO, tea_import_io },
    { TEA_MODULE_RANDOM, tea_import_random },
    { NULL, NULL }
};

GCstr* tea_imp_dirname(tea_State* T, char* path, int len) 
{
    if(!len) 
    {
        return tea_str_lit(T, ".");
    }

    char* sep = path + len;

    /* Trailing slashes */
    while(sep != path) 
    {
        if(0 == IS_DIR_SEP(*sep))
            break;
        sep--;
    }

    /* First found */
    while(sep != path) 
    {
        if(IS_DIR_SEP(*sep))
            break;
        sep--;
    }

    /* Trim again */
    while(sep != path) 
    {
        if(0 == IS_DIR_SEP(*sep))
            break;
        sep--;
    }

    if(sep == path && !IS_DIR_SEP(*sep)) 
    {
        return tea_str_lit(T, ".");
    }

    len = sep - path + 1;

    return tea_str_copy(T, path, len);
}

bool tea_imp_resolvepath(char* directory, char* path, char* ret) 
{
    char buf[PATH_MAX];

    snprintf(buf, PATH_MAX, "%s%c%s", directory, DIR_SEP, path);

#if TEA_TARGET_WINDOWS
    _fullpath(ret, buf, PATH_MAX);
#else
    if(realpath(buf, ret) == NULL) 
    {
        return false;
    }
#endif

    return true;
}

GCstr* tea_imp_getdir(tea_State* T, char* source) 
{
    char res[PATH_MAX];
    if(!tea_imp_resolvepath(".", source, res)) 
    {
        tea_err_run(T, TEA_ERR_PATH, source);
    }

    return tea_imp_dirname(T, res, strlen(res));
}

static void call_native_module(tea_State* T, int index)
{
    tea_push_cfunction(T, modules[index].fn, 0);
    tea_call(T, 0);
}

static int find_native_module(char* name, int len)
{
    for(int i = 0; modules[i].name != NULL; i++) 
    {
        if(strncmp(modules[i].name, name, len) == 0) 
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

#if TEA_TARGET_WINDOWS
#define SHARED_EXT  ".dll"
#else
#define SHARED_EXT  ".so"
#endif

static GCstr* resolve_filename(tea_State* T, char* dir, char* path_name)
{
    GCstr* file = NULL;

    const char* exts[] = { ".tea", ".tbc", SHARED_EXT, "/init.tea" };
    const int n = sizeof(exts) / sizeof(exts[0]);

    size_t l;
    for(int i = 0; i < n; i++) 
    {
        l = strlen(path_name) + strlen(exts[i]);
        char* filename = tea_mem_new(T, char, l + 1);
        sprintf(filename, "%s%s", path_name, exts[i]);

        char path[PATH_MAX];
        if(tea_imp_resolvepath(dir, filename, path))
        {
            if(readable(path))
            {
                file = tea_str_copy(T, path, strlen(path));
                tea_mem_freevec(T, char, filename, l + 1);
                break;
            }
        }

        tea_mem_freevec(T, char, filename, l + 1);
    }

    return file;
}

void tea_imp_relative(tea_State* T, GCstr* dir, GCstr* path_name)
{
    GCstr* path = resolve_filename(T, dir->chars, path_name->chars);
    if(path == NULL)
    {
        tea_err_run(T, TEA_ERR_NOPATH, path_name->chars);
    }

    TValue v;
    if(tea_tab_get(&T->modules, path, &v)) 
    {
        T->last_module = AS_MODULE(v);
        tea_vm_push(T, NULL_VAL);
        return;
    }

    GCmodule* module = tea_obj_new_module(T, path);
    module->path = tea_imp_dirname(T, path->chars, path->len);
    T->last_module = module;

    int status = tea_load_file(T, path->chars);
    if(status == TEA_ERROR_FILE)
    {
        tea_err_run(T, TEA_ERR_NOPATH, path_name->chars);
    }

    if(status != TEA_OK)
    {
        tea_pop(T, 1);
        tea_err_throw(T, TEA_ERROR_SYNTAX);
    }

    tea_call(T, 0);
}

void tea_imp_logical(tea_State* T, GCstr* name)
{
    TValue v;
    if(tea_tab_get(&T->modules, name, &v))
    {
        T->last_module = AS_MODULE(v);
        tea_vm_push(T, v);
        return;
    }

    int index = find_native_module(name->chars, name->len);
    if(index != -1) 
    {
        call_native_module(T, index);
    }
    else
    {
        GCstr* module = resolve_filename(T, ".", name->chars);
        if(module == NULL)
        {
            tea_err_run(T, TEA_ERR_NOMOD, name->chars);
        }

        printf("shared %s\n", module->chars);
        if(get_filename_ext(module->chars, SHARED_EXT))
        {
            const char* symname = tea_push_fstring(T, TEA_LL_SYM "%s", name->chars);

            void* lib = ll_load(T, module->chars);
            tea_CFunction fn = ll_sym(T, lib, symname);
            T->top--;

            tea_push_cfunction(T, fn, 0);
            tea_call(T, 0);
        }
        else
        {
            tea_err_run(T, TEA_ERR_NOMOD, name->chars);
        }
    }

    TValue module = T->top[-1];
    T->last_module = AS_MODULE(module);
}