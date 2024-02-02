/*
** tea_loadlib.c
** Dynamic library loader
*/

#define tea_loadlib_c
#define TEA_CORE

#include "tea.h"

#include "tea_def.h"
#include "tea_arch.h"

#if TEA_TARGET_DLOPEN

#include <dlfcn.h>

void tea_ll_unload(void* lib)
{
    dlclose(lib);
}

void* tea_ll_load(tea_State* T, const char* path)
{
    void* lib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if(lib == NULL)
        tea_error(T, dlerror());
    return lib;
}

tea_CFunction tea_ll_sym(tea_State* T, void* lib, const char* sym)
{
    tea_CFunction f = (tea_CFunction)dlsym(lib, sym);
    if(f == NULL)
        tea_error(T, dlerror());
    return f;
}

#elif TEA_TARGET_WINDOWS

#include <windows.h>

static void lib_error(tea_State* T)
{
    int error = GetLastError();
    char buffer[128];
    if(FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, buffer, sizeof(buffer) / sizeof(char), NULL))
        tea_error(T, buffer);
    else
        tea_error(T, "system error %d\n", error);
}

void tea_ll_unload(void* lib)
{
    FreeLibrary((HMODULE)lib);
}

void* tea_ll_load(tea_State* T, const char* path)
{
    HMODULE lib = LoadLibraryExA(path, NULL, 0);
    if(lib == NULL)
        lib_error(T);
    return lib;
}

tea_CFunction tea_ll_sym(tea_State* T, void* lib, const char* sym)
{
    tea_CFunction f = (tea_CFunction)GetProcAddress((HMODULE)lib, sym);
    if(f == NULL)
        lib_error(T);
    return f;
}

#else

#define DLMSG   "dynamic libraries not enabled"

void tea_ll_unload(void* lib)
{
    /* Not used */
    UNUSED(lib);
}

void* tea_ll_load(tea_State* T, const char* path)
{
    UNUSED(path);
    tea_error(T, DLMSG);
    return NULL;
}

tea_CFunction tea_ll_sym(tea_State* T, void* lib, const char* sym)
{
    UNUSED(lib); UNUSED(sym);
    tea_error(T, DLMSG);
    return NULL;
}

#endif