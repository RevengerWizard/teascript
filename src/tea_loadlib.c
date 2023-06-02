/*
** tea_loadlib.c
** Dynamic library loader for Teascript
*/

#if defined(_WIN32)
#include <windows.h>
#endif

#define tea_loadlib_c
#define TEA_CORE

#include "tea.h"

#if defined(TEA_USE_DLOPEN)

#include <dlfcn.h>

void tea_ll_unload(void* lib)
{
    dlclose(lib);
}

void* tea_ll_load(TeaState* T, const char* path)
{
    void* lib = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
    if(lib == NULL)
        tea_error(T, dlerror());
    return lib;
}

TeaCFunction tea_ll_sym(TeaState* T, void* lib, const char* sym)
{
    TeaCFunction f = (TeaCFunction)dlsym(lib, sym);
    if(f == NULL)
        tea_error(T, dlerror());
    return f;
}

#elif defined(_WIN32)

static void lib_error(TeaState* T)
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

void* tea_ll_load(TeaState* T, const char* path)
{
    HMODULE lib = LoadLibraryExA(path, NULL, 0);
    if(lib == NULL)
        lib_error(T);
    return lib;
}

TeaCFunction tea_ll_sym(TeaState* T, void* lib, const char* sym)
{
    TeaCFunction f = (TeaCFunction)GetProcAddress((HMODULE)lib, sym);
    if(f == NULL)
        lib_error(T);
    return f;
}

#else

#define DLMSG   "dynamic libraries not enabled"

void tea_ll_unload(void* lib)
{
    /* not used */
}

void* tea_ll_load(TeaState* T, const char* path)
{
    tea_error(T, DLMSG);
    return NULL;
}

TeaCFunction tea_ll_sym(TeaState* T, void* lib, const char* sym)
{
    tea_error(T, DLMSG);
    return NULL;
}

#endif