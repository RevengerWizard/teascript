/*
** tea_import.c
** Import loader
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define tea_import_c
#define TEA_CORE

#include "tea.h"
#include "tealib.h"

#include "tea_arch.h"
#include "tea_import.h"
#include "tea_str.h"
#include "tea_err.h"
#include "tea_tab.h"
#include "tea_gc.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if TEA_TARGET_WINDOWS
#define DIR_SEP '\\'
#define DIR_ALT_SEP '/'

#define TEA_PATH_LIB "lib"
#define TEA_PATH_PACKAGE "package"
#define TEA_PATH_SCRIPT "script"
#else
#define DIR_SEP '/'

#define TEA_PATH_LIB "/usr/lib/teascript/lib"
#define TEA_PATH_PACKAGE "/usr/lib/teascript/package"
#define TEA_PATH_SCRIPT "/usr/lib/teascript/script"
#endif

#ifdef DIR_ALT_SEP
#define IS_DIR_SEP(c) ((c) == DIR_SEP || (c) == DIR_ALT_SEP)
#else
#define IS_DIR_SEP(c) (c == DIR_SEP)
#endif

#define setprogdir(T) (tea_push_literal(T, ""))

#define TEA_IMPORT_SYM "tea_import_%s"

typedef struct tea_handle
{
    void* lib;
    struct tea_handle* next;
} tea_handle;

/* ------------------------------------------------------------------------ */

#if TEA_TARGET_DLOPEN

#include <dlfcn.h>

static void ll_unloadlib(void* lib)
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#undef setprogdir

static const char* setprogdir(tea_State* T)
{
    char buff[MAX_PATH + 1];
    char* lb = NULL;
    DWORD nsize = sizeof(buff);
    DWORD n = GetModuleFileNameA(NULL, buff, nsize);
    if(n == 0 || n == nsize || (lb = strrchr(buff, '\\')) == NULL)
    {
        tea_error(T, "unable to get ModuleFileName");
    }
    *lb = '\0';
    return tea_push_lstring(T, buff, nsize);
}

static void ll_error(tea_State* T)
{
    int error = GetLastError();
    char buffer[128];
    if(FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, buffer, sizeof(buffer) / sizeof(char), NULL))
        tea_error(T, buffer);
    else
        tea_error(T, "system error %d\n", error);
}

static void ll_unloadlib(void* lib)
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

static void ll_unloadlib(void* lib)
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

static void ll_register(tea_State* T, void* h)
{
    tea_handle* hh = (tea_handle*)tea_mem_new(T, sizeof(tea_handle));
    hh->lib = h;
    hh->next = T->handle;
    T->handle = hh;
}

/* ------------------------------------------------------------------------ */

static const tea_Reg modules[] = {
    { TEA_MODULE_MATH, tea_import_math },
    { TEA_MODULE_TIME, tea_import_time },
    { TEA_MODULE_OS, tea_import_os },
    { TEA_MODULE_SYS, tea_import_sys },
    { TEA_MODULE_IO, tea_import_io },
    { TEA_MODULE_RANDOM, tea_import_random },
    { TEA_MODULE_DEBUG, tea_import_debug },
    { TEA_MODULE_UTF8, tea_import_utf8 },
    { NULL, NULL }
};

GCstr* tea_imp_dirname(tea_State* T, char* path, int len)
{
    if(!len)
    {
        return tea_str_newlit(T, ".");
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
        return tea_str_newlit(T, ".");
    }

    len = sep - path + 1;

    return tea_str_new(T, path, len);
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
        tea_err_callerv(T, TEA_ERR_PATH, source);
    }
    return tea_imp_dirname(T, res, strlen(res));
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

static GCstr* imp_search_path(tea_State* T, char* dir, char* path_name)
{
    GCstr* file = NULL;

    const char* exts[] = {".tea", ".tbc", SHARED_EXT, "/init.tea"};
    const int n = sizeof(exts) / sizeof(exts[0]);

    size_t l;
    for(int i = 0; i < n; i++)
    {
        l = strlen(path_name) + strlen(exts[i]);
        char* filename = tea_mem_newvec(T, char, l + 1);
        sprintf(filename, "%s%s", path_name, exts[i]);

        char path[PATH_MAX];
        if(tea_imp_resolvepath(dir, filename, path))
        {
            if(readable(path))
            {
                file = tea_str_new(T, path, strlen(path));
                tea_mem_freevec(T, char, filename, l + 1);
                break;
            }
        }

        tea_mem_freevec(T, char, filename, l + 1);
    }

    return file;
}

/* Returns true if a loader handled it; leaves the module on the stack */
static bool imp_custom_loader(tea_State* T, GCstr* dir, GCstr* path_name)
{
    if(!tea_tab_get(&T->modules, tea_str_newlit(T, TEA_MODULE_SYS)))
        return false;

    tea_get_var(T, TEA_MODULE_SYS, "loaders");
    if(!tea_is_list(T, -1))
    {
        T->top--;
        return false;
    }

    GClist* loaders = listV(T->top - 1);       /* rooted on the stack */
    for(size_t i = 0; i < loaders->len; i++)
    {
        TValue* slot = list_slot(loaders, i);
        if(!tvisfunc(slot))
            continue;

        copyTV(T, T->top, slot);
        incr_top(T);
        tea_push_string(T, str_data(path_name));
        tea_push_string(T, str_data(dir));
        tea_call(T, 2);

        TValue* res = T->top - 1;               /* [loaders, res] */
        if(tvisnil(res))
        {
            T->top--;
            continue;
        }

        if(!tvislist(res) || listV(res)->len != 2 ||
           !tvisstr(list_slot(listV(res), 0)) || !tvisfunc(list_slot(listV(res), 1)))
        {
            tea_error(T, "resolver must return nil or a list [key, load]");
        }

        GClist* pair = listV(res);              /* rooted by res */
        GCstr* key = strV(list_slot(pair, 0));  /* rooted by pair */

        /* Cache hit: no loading at all */
        TValue* cached = tea_tab_get(&T->modules, key);
        if(cached)
        {
            GCmodule* module = moduleV(cached);
            T->last_module = module;
            T->top -= 2;                        /* res, loaders */
            setmoduleV(T, T->top++, module);
            return true;
        }

        /* Miss: load(key) */
        copyTV(T, T->top, list_slot(pair, 1));
        incr_top(T);
        setstrV(T, T->top++, key);
        tea_call(T, 1);                         /* [loaders, res, fn] */

        TValue* fn = T->top - 1;
        if(!tvisfunc(fn) || !isteafunc(funcV(fn)))
            tea_error(T, "loader must return a Teascript function");

        GCmodule* module = funcV(fn)->t.module; /* rooted by the function */
        module->path = tea_imp_dirname(T, str_datawr(key), key->len);

        /* Register before running, so circular imports see it */
        setmoduleV(T, tea_tab_set(T, &T->modules, key), module);

        T->last_module = module;
        int status = tea_pcall(T, 0);
        if(status != TEA_OK)
        {
            tea_tab_delete(&T->modules, key);    /* Don't keep a half-built module */
            tea_err_throw(T, status);
        }

        T->top -= 3;                            /* result, res, loaders */
        setmoduleV(T, T->top++, module);
        return true;
    }

    T->top--;                                   /* loaders list */
    return false;
}

static GCmodule* imp_module_load(tea_State* T, GCstr* path)
{
    GCmodule* module = tea_module_new(T, path);
    module->path = tea_imp_dirname(T, str_datawr(path), path->len);
    T->last_module = module;

    int status = tea_load_file(T, str_data(path), str_data(path));
    if(status != TEA_OK)
    {
        /* Rethrow the syntax error */
        tea_err_throw(T, TEA_ERROR_SYNTAX);
    }
    tea_call(T, 0);
    return module;
}

void tea_imp_relative(tea_State* T, GCstr* dir, GCstr* path_name)
{
    if(imp_custom_loader(T, dir, path_name))
        return;

    GCstr* path = imp_search_path(T, str_datawr(dir), str_datawr(path_name));
    if(!path)
    {
        tea_err_callerv(T, TEA_ERR_NOPATH, str_data(path_name));
    }

    TValue* o = tea_tab_get(&T->modules, path);
    if(o)
    {
        T->last_module = moduleV(o);
        copyTV(T, T->top++, o);
        return;
    }
    imp_module_load(T, path);
}

void tea_imp_logical(tea_State* T, GCstr* name)
{
    TValue* o = tea_tab_get(&T->modules, name);
    if(o)
    {
        T->last_module = moduleV(o);
        copyTV(T, T->top++, o);
        return;
    }

    for(int i = 0; modules[i].name; i++)
    {
        if(strlen(modules[i].name) == name->len &&
           memcmp(modules[i].name, str_data(name), name->len) == 0)
        {
            tea_push_cfunction(T, modules[i].fn, 0, 0);
            tea_call(T, 0);
            T->last_module = moduleV(T->top - 1);
            return;
        }
    }

    const char* exe = setprogdir(T);
    GCstr* dir = tea_str_newlen(T, exe);
    T->top--;

    const char* exts[] = {TEA_PATH_SCRIPT, TEA_PATH_LIB, TEA_PATH_PACKAGE};
    const int n = sizeof(exts) / sizeof(exts[0]);

    GCstr* path;
    for(int i = 0; i < n; i++)
    {
        const char* x = tea_push_fstring(T, "%s%c%s", exts[i], DIR_SEP, str_data(name));
        path = imp_search_path(T, str_datawr(dir), (char*)x);
        tea_pop(T, 1);
        if(path)
            break;
    }

    if(!path)
    {
        tea_err_callerv(T, TEA_ERR_NOPATH, str_data(name));
    }

    o = tea_tab_get(&T->modules, path);
    if(o)
    {
        T->last_module = moduleV(o);
        copyTV(T, T->top++, o);
        return;
    }

    if(get_filename_ext(str_data(path), SHARED_EXT))
    {
        const char* symname = tea_push_fstring(T, TEA_IMPORT_SYM, str_data(name));

        void* lib = ll_load(T, str_data(path));
        ll_register(T, lib);
        tea_CFunction fn = ll_sym(T, lib, symname);
        T->top--;

        tea_push_cfunction(T, fn, 0, 0);
        tea_call(T, 0);

        T->last_module = moduleV(T->top - 1);
        return;
    }

    GCmodule* module = imp_module_load(T, path);
    T->top--;

    setmoduleV(T, T->top++, module);
}

void tea_imp_freehandle(tea_State* T)
{
    tea_handle* h = T->handle;
    while(h)
    {
        tea_handle* next = h->next;
        ll_unloadlib(h->lib);
        tea_mem_free(T, h, sizeof(tea_handle));
        h = next;
    }
}
