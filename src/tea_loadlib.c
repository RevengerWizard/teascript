// tea_loadlib.c
// Teascript dynamic library loading

#include "tea.h"

#define TEA_POF	"tea_import_"

typedef void (*voidf)(void);

static void tsys_unload(void* lib);

static void* tsys_load(TeaState* T, const char* path, int seeglb);

static TeaCFunction tsys_sym(TeaState* T, void* lib, const char* sym);

#if defined(TEA_USE_DLOPEN)

#if defined(__GNUC__)
#define cast_func(p) (__extension__ (TeaCFunction)(p))
#else
#define cast_func(p) ((TeaCFunction)(p))
#endif

#include <dlfcn.h>

static void tsys_unload(void* lib)
{
    dlclose(lib);
}

static void* tsys_load(TeaState* T, const char* path, int seeglb)
{
    void* lib = dlopen(path, RTLD_NOW | (seeglb ? RTLD_GLOBAL : RTLD_LOCAL));
    if(lib == NULL) // error
    return lib;
}

static TeaCFunction tsys_sym(TeaState* T, void* lib, const char* sym)
{
    lua_CFunction f = cast_func(dlsym(lib, sym));
    if(f == NULL) // error
    return f;
}

#elif defined(TEA_DL_DLL)

#include <windows.h>

static void tsys_unload(void* lib)
{
    FreeLibrary((HMODULE)lib);
}

static void* tsys_load(TeaState* T, const char* path, int seeglb)
{
    HMODULE lib = LoadLibraryExA(path, NULL, 0);
    (void)(seeglb);  // not used: symbols are 'global' by default
    if(lib == NULL) // error
    return lib;
}

static TeaCFunction tsys_sym(TeaState* T, void* lib, const char* sym)
{
    TeaCFunction f = (TeaCFunction)(voidf)GetProcAddress((HMODULE)lib, sym);
    if(f == NULL) // error;
    return f;
}

#else

static void tsys_unload(void* lib)
{
    (void)(lib);
}

static void* tsys_load(TeaState* T, const char* path, int seeglb)
{
    (void)path;
    (void)seeglb;
    return NULL;
}

static TeaCFunction tsys_sym(TeaState* T, void* lib, const char* sym)
{
    (void)lib;
    (void)sym;
    return NULL;
}

#endif