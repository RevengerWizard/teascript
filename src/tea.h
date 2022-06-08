#ifndef TEA_H
#define TEA_H
/*
#include "tea_config.h"

typedef struct TeaFunction
{
    const char* name;
    TeaNativeFunction fn;
} TeaFunction;

typedef struct TeaMethod
{
    const char* name;
    const char* type;
    TeaNativeFunction fn;
} TeaMethod;

typedef void (*TeaNativeFunction)(TeaState* T);

typedef void* (*TeaReallocateFunction)(void* memory, size_t new_size);

typedef struct TeaState TeaState;
typedef struct TeaHandle TeaHandle;
typedef enum TeaInterpretResult TeaInterpretResult;

typedef enum
{
    TEA_TYPE_UNKNOWN,
    TEA_TYPE_NULL,
    TEA_TYPE_NUMBER,
    TEA_TYPE_BOOL,
    TEA_TYPE_RANGE,
    TEA_TYPE_LIST,
    TEA_TYPE_MAP,
    TEA_TYPE_STRING,
    TEA_TYPE_CLASS,
    TEA_TYPE_INSTANCE,
    TEA_TYPE_MODULE
} TeaType;

TEA_API int tea_version();

TEA_API TeaState* tea_open();
TEA_API void tea_close(TeaState* T);
TEA_API void tea_setargs(TeaState* T, int argc, const char* argv);

TEA_API int tea_type(TeaState* T, int slot);
TEA_API const char* tea_typename(TeaState* T, int slot);

TEA_API TeaHandle* tea_gethandle(TeaState* T, int slot);
TEA_API double tea_getnumber(TeaState* T, int slot);
TEA_API int tea_getbool(TeaState* T, int slot);
TEA_API void tea_getrange(TeaState* T, int slot, double* from, double* to);
TEA_API const char* tea_getlstring(TeaState* T, int slot, int* len);
TEA_API const char* tea_getstring(TeaState* T, int slot);

TEA_API void tea_sethandle(TeaState* T, int slot, TeaHandle* handle);
TEA_API void tea_setnull(TeaState* T, int slot);
TEA_API void tea_setnumber(TeaState* T, int slot, double n);
TEA_API void tea_setbool(TeaState* T, int slot, int b);
TEA_API const char* tea_setlstring(TeaState* T, int slot, const char* s, int len);
TEA_API const char* tea_setstring(TeaState* T, int slot, const char* s);

TEA_API void tea_newlist(TeaState* T, int slot);
TEA_API void tea_newmap(TeaState* T, int slot);
TEA_API void tea_newinstance(TeaState* T, int slot);
TEA_API void tea_newclass(TeaState* T, int slot);

TEA_API int tea_len(TeaState* T, int slot);

TEA_API void tea_getlistitem(TeaState* T, int slot);
TEA_API void tea_setlistitem(TeaState* T, int slot);

TEA_API void tea_getglobal(TeaState* T, const char* name, int slot);
TEA_API void tea_setglobal(TeaState* T, const char* name, int slot);

TEA_API double tea_checknumber(TeaState* T, int slot);
TEA_API const char* tea_checklstring(TeaState* T, int slot, int* len);
TEA_API const char* tea_checkstring(TeaState* T, int slot);
TEA_API void tea_checklist(TeaState* T, int slot);
TEA_API void tea_checkmap(TeaState* T, int slot);

//TEA_API TeaInterpretResult tea_interpret(TeaState* T, int slot);
//TEA_API TeaInterpretResult tea_call(TeaState* T, int slot);
TEA_API void tea_release(TeaState* T, TeaHandle* handle);

//TEA_API void tea_error(TeaState* T, const char* message);

#define tea_isunknown(T, n) (tea_type(T, (n)) == TEA_TYPE_UNKNOWN)
#define tea_isnull(T, n) (tea_type(T, (n)) == TEA_TYPE_NULL)
#define tea_isnumber(T, n) (tea_type(L, (n)) == TEA_TYPE_NUMBER)
#define tea_isbool(T, n) (tea_type(T, (n)) == TEA_TYPE_BOOL)
#define tea_isstring(T, n) (tea_type(T, (n)) == TEA_TYPE_STRING)
#define tea_islist(T, n) (tea_type(T, (n)) == TEA_TYPE_LIST)
#define tea_ismap(T, n) (tea_type(T, (n)) == TEA_TYPE_MAP)
*/
#endif