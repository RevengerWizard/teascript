/*
** tea.h
** Teascript C API
*/

#ifndef _TEA_H
#define _TEA_H

#include <stdarg.h>
#include <stddef.h>
#include <stdbool.h>

#include "teaconf.h"

#define TEA_REPOSITORY "https://github.com/RevengerWizard/teascript"

#define TEA_VERSION_MAJOR 0
#define TEA_VERSION_MINOR 0
#define TEA_VERSION_PATCH 0

#define TEA_VERSION_NUMBER (TEA_VERSION_MAJOR * 10000 + TEA_VERSION_MINOR * 100 + TEA_VERSION_PATCH)

#define TEA_VERSION "0.0.0"

/* Mark for precompiled code ('<esc>Tea') */
#define TEA_SIGNATURE "\x1bTea"

/* Minimum Teascript stack available to a C function */
#define TEA_MIN_STACK 20

/* 
** Pseudo-indeces
*/
#define TEA_UPVALUES_INDEX (-10000)
#define TEA_MODULE_INDEX (-10000)
#define tea_upvalue_index(i) (TEA_UPVALUES_INDEX - (i))

/* Type of numbers in Teascript */
typedef double tea_Number;

/* Type for integer functions */
typedef ptrdiff_t tea_Integer;

/*
** Interpreter status
*/
enum
{
    TEA_OK,
    TEA_ERROR_SYNTAX,
    TEA_ERROR_RUNTIME,
    TEA_ERROR_MEMORY,
    TEA_ERROR_FILE,
    TEA_ERROR_ERROR
};

typedef struct tea_State tea_State;

typedef void (*tea_CFunction)(tea_State* T);

typedef void (*tea_Finalizer)(void* p);

/*
** Functions that read/write blocks when loading/dumping Teascript code
*/
typedef const char* (*tea_Reader)(tea_State* T, void* ud, size_t* sz);

typedef int (*tea_Writer)(tea_State* T, void* ud, const void* p, size_t sz);

/*
** Prototype for memory allocation functions
*/
typedef void* (*tea_Alloc)(void* ud, void* ptr, size_t osize, size_t nsize);

typedef struct tea_Reg
{
    const char* name;
    tea_CFunction fn;
    int nargs;
} tea_Reg;

typedef struct tea_Methods
{
    const char* name;
    const char* type;
    tea_CFunction fn;
    int nargs;
} tea_Methods;

/*
** Basic type masks
*/
#define TEA_MASK_NONE       (1 << TEA_TYPE_NONE)
#define TEA_MASK_NIL       (1 << TEA_TYPE_NIL)
#define TEA_MASK_BOOL       (1 << TEA_TYPE_BOOL)
#define TEA_MASK_NUMBER     (1 << TEA_TYPE_NUMBER)
#define TEA_MASK_POINTER    (1 << TEA_TYPE_POINTER)
#define TEA_MASK_STRING     (1 << TEA_TYPE_STRING)
#define TEA_MASK_RANGE      (1 << TEA_TYPE_RANGE)
#define TEA_MASK_FUNCTION   (1 << TEA_TYPE_FUNCTION)
#define TEA_MASK_MODULE     (1 << TEA_TYPE_MODULE)
#define TEA_MASK_CLASS      (1 << TEA_TYPE_CLASS)
#define TEA_MASK_INSTANCE   (1 << TEA_TYPE_INSTANCE)
#define TEA_MASK_LIST       (1 << TEA_TYPE_LIST)
#define TEA_MASK_MAP        (1 << TEA_TYPE_MAP)
#define TEA_MASK_USERDATA   (1 << TEA_TYPE_USERDATA)

/* Option for variadic functions */
#define TEA_VARARGS (-1)

/*
** Basic types
*/
enum
{
    TEA_TYPE_NONE,
    TEA_TYPE_NIL,
    TEA_TYPE_BOOL,
    TEA_TYPE_NUMBER,
    TEA_TYPE_POINTER,
    TEA_TYPE_STRING,
    TEA_TYPE_RANGE,
    TEA_TYPE_FUNCTION,
    TEA_TYPE_MODULE,
    TEA_TYPE_CLASS,
    TEA_TYPE_INSTANCE,
    TEA_TYPE_LIST,
    TEA_TYPE_MAP,
    TEA_TYPE_USERDATA,
};

/*
** State manipulation
*/
TEA_API tea_State* tea_new_state(tea_Alloc allocf, void* ud);
TEA_API void tea_close(tea_State* T);
TEA_API void tea_set_argv(tea_State* T, int argc, char** argv, int argf);
TEA_API void tea_set_repl(tea_State* T, bool b);

TEA_API tea_CFunction tea_atpanic(tea_State* T, tea_CFunction panicf);

TEA_API tea_Alloc tea_get_allocf(tea_State* T, void** ud);
TEA_API void tea_set_allocf(tea_State* T, tea_Alloc f, void* ud);

/*
** Basic stack manipulation
*/
TEA_API int tea_absindex(tea_State* T, int index);
TEA_API void tea_pop(tea_State* T, int n);
TEA_API int tea_get_top(tea_State* T);
TEA_API void tea_set_top(tea_State* T, int index);
TEA_API void tea_push_value(tea_State* T, int index);
TEA_API void tea_remove(tea_State* T, int index);
TEA_API void tea_insert(tea_State* T, int index);
TEA_API void tea_replace(tea_State* T, int index);
TEA_API void tea_copy(tea_State* T, int from_index, int to_index);

/*
** Get functions (stack -> C)
*/
TEA_API int tea_get_mask(tea_State* T, int index);
TEA_API int tea_get_type(tea_State* T, int index);
TEA_API bool tea_get_bool(tea_State* T, int index);
TEA_API tea_Number tea_get_number(tea_State* T, int index);
TEA_API tea_Integer tea_get_integer(tea_State* T, int index);
TEA_API const void* tea_get_pointer(tea_State* T, int index);
TEA_API void tea_get_range(tea_State* T, int index, tea_Number* start, tea_Number* end, tea_Number* step);
TEA_API const char* tea_get_lstring(tea_State* T, int index, size_t* len);
TEA_API const char* tea_get_string(tea_State* T, int index);
TEA_API void* tea_get_userdata(tea_State* T, int index);

TEA_API void tea_set_finalizer(tea_State* T, tea_Finalizer f);

/*
** Access functions (stack -> C)
*/
TEA_API bool tea_is_object(tea_State* T, int index);
TEA_API bool tea_is_cfunction(tea_State* T, int index);

TEA_API const char* tea_typeof(tea_State* T, int index);

TEA_API bool tea_to_bool(tea_State* T, int index);
TEA_API tea_Number tea_to_numberx(tea_State* T, int index, bool* is_num);
TEA_API tea_Number tea_to_number(tea_State* T, int index);
TEA_API tea_Integer tea_to_integerx(tea_State* T, int index, bool* is_num);
TEA_API tea_Integer tea_to_integer(tea_State* T, int index);
TEA_API const void* tea_to_pointer(tea_State* T, int index);
TEA_API void* tea_to_userdata(tea_State* T, int index);
TEA_API const char* tea_to_lstring(tea_State* T, int index, size_t* len);
TEA_API const char* tea_to_string(tea_State* T, int index);
TEA_API tea_CFunction tea_to_cfunction(tea_State* T, int index);

TEA_API bool tea_equal(tea_State* T, int index1, int index2);
TEA_API bool tea_rawequal(tea_State* T, int index1, int index2);

/*
** Push functions (C -> stack)
*/
TEA_API void tea_push_nil(tea_State* T);
TEA_API void tea_push_true(tea_State* T);
TEA_API void tea_push_false(tea_State* T);
TEA_API void tea_push_bool(tea_State* T, bool b);
TEA_API void tea_push_number(tea_State* T, tea_Number n);
TEA_API void tea_push_integer(tea_State* T, tea_Integer n);
TEA_API void tea_push_pointer(tea_State* T, void* p);
TEA_API const char* tea_push_lstring(tea_State* T, const char* s, size_t len);
TEA_API const char* tea_push_string(tea_State* T, const char* s);
TEA_API const char* tea_push_fstring(tea_State* T, const char* fmt, ...);
TEA_API const char* tea_push_vfstring(tea_State* T, const char* fmt, va_list args);
TEA_API void tea_push_range(tea_State* T, tea_Number start, tea_Number end, tea_Number step);
TEA_API void tea_push_cclosure(tea_State* T, tea_CFunction fn, int nargs, int nupvalues);
TEA_API void tea_push_cfunction(tea_State* T, tea_CFunction fn, int nargs);

TEA_API void tea_new_list(tea_State* T);
TEA_API void tea_new_map(tea_State* T);
TEA_API void* tea_new_userdata(tea_State* T, size_t size);
TEA_API void tea_new_class(tea_State* T, const char* name);
TEA_API void tea_new_module(tea_State* T, const char* name);

TEA_API void tea_create_class(tea_State* T, const char* name, const tea_Methods* klass);
TEA_API void tea_create_module(tea_State* T, const char* name, const tea_Reg* module);

TEA_API int tea_len(tea_State* T, int index);

TEA_API void tea_add_item(tea_State* T, int list);
TEA_API bool tea_get_item(tea_State* T, int list, int index);
TEA_API bool tea_set_item(tea_State* T, int list, int index);
TEA_API bool tea_del_item(tea_State* T, int list, int index);
TEA_API bool tea_insert_item(tea_State* T, int list, int index);

TEA_API bool tea_get_field(tea_State* T, int obj);
TEA_API void tea_set_field(tea_State* T, int obj);
TEA_API bool tea_del_field(tea_State* T, int obj);
TEA_API bool tea_get_key(tea_State* T, int obj, const char* key);
TEA_API void tea_set_key(tea_State* T, int obj, const char* key);
TEA_API bool tea_del_key(tea_State* T, int obj, const char* key);

TEA_API bool tea_get_attr(tea_State* T, int obj, const char* key);
TEA_API void tea_set_attr(tea_State* T, int obj, const char* key);

TEA_API bool tea_get_global(tea_State* T, const char* name);
TEA_API void tea_set_global(tea_State* T, const char* name);

TEA_API void tea_set_funcs(tea_State* T, const tea_Reg* reg, int nup);
TEA_API void tea_set_methods(tea_State* T, const tea_Methods* reg, int nup);

TEA_API bool tea_test_stack(tea_State* T, int size);
TEA_API void tea_check_stack(tea_State* T, int size, const char* msg);

TEA_API void tea_check_type(tea_State* T, int index, int type);
TEA_API void tea_check_any(tea_State* T, int index);
TEA_API bool tea_check_bool(tea_State* T, int index);
TEA_API tea_Number tea_check_number(tea_State* T, int index);
TEA_API tea_Integer tea_check_integer(tea_State* T, int index);
TEA_API const void* tea_check_pointer(tea_State* T, int index);
TEA_API void tea_check_range(tea_State* T, int index, tea_Number* start, tea_Number* end, tea_Number* step);
TEA_API const char* tea_check_lstring(tea_State* T, int index, size_t* len);
TEA_API const char* tea_check_string(tea_State* T, int index);
TEA_API tea_CFunction tea_check_cfunction(tea_State* T, int index);
TEA_API void* tea_check_userdata(tea_State* T, int index);
TEA_API int tea_check_option(tea_State* T, int index, const char* def, const char* const options[]);

TEA_API void tea_opt_null(tea_State* T, int index);
TEA_API bool tea_opt_bool(tea_State* T, int index, bool def);
TEA_API tea_Number tea_opt_number(tea_State* T, int index, tea_Number def);
TEA_API tea_Integer tea_opt_integer(tea_State* T, int index, tea_Integer def);
TEA_API const void* tea_opt_pointer(tea_State* T, int index, void* def);
TEA_API const char* tea_opt_lstring(tea_State* T, int index, const char* def, size_t* len);
TEA_API const char* tea_opt_string(tea_State* T, int index, const char* def);
TEA_API void* tea_opt_userdata(tea_State* T, int index, void* def);
TEA_API tea_CFunction tea_opt_cfunction(tea_State* T, int index, tea_CFunction def);

/*
** Garbage collection function
*/
TEA_API int tea_gc(tea_State* T);

TEA_API void* tea_alloc(tea_State* T, size_t size);
TEA_API void* tea_realloc(tea_State* T, void* p, size_t size);
TEA_API void tea_free(tea_State* T, void* p);

/*
** 'load' and 'call' functions (load and run Teascript code)
*/
TEA_API void tea_call(tea_State* T, int n);
TEA_API int tea_pcall(tea_State* T, int n);
TEA_API int tea_pccall(tea_State* T, tea_CFunction func, void* ud);

TEA_API int tea_loadx(tea_State* T, tea_Reader reader, void* data, const char* name, const char* mode);
TEA_API int tea_load(tea_State* T, tea_Reader reader, void* data, const char* name);
TEA_API int tea_dump(tea_State* T, tea_Writer writer, void* data);

TEA_API int tea_load_filex(tea_State* T, const char* filename, const char* name, const char* mode);
TEA_API int tea_load_file(tea_State* T, const char* filename, const char* name);
TEA_API int tea_load_bufferx(tea_State* T, const char* buffer, size_t size, const char* name, const char* mode);
TEA_API int tea_load_buffer(tea_State* T, const char* buffer, size_t size, const char* name);
TEA_API int tea_load_string(tea_State* T, const char* s);

TEA_API int tea_error(tea_State* T, const char* fmt, ...);

TEA_API void tea_concat(tea_State* T);

/*
** Some useful macros
*/

#define tea_open()  tea_new_state(NULL, NULL)

#define tea_push_literal(T, s)  tea_push_lstring(T, "" s, (sizeof(s)/sizeof(char))-1)

#define tea_check_args(T, cond, msg, ...) ((void)((cond) && tea_error(T, (msg), __VA_ARGS__)))

#define tea_check_list(T, index) tea_check_type(T, (index), TEA_TYPE_LIST)
#define tea_check_function(T, index) tea_check_type(T, (index), TEA_TYPE_FUNCTION)
#define tea_check_map(T, index) tea_check_type(T, (index), TEA_TYPE_MAP)
#define tea_check_instance(T, index) tea_check_type(T, (index), TEA_TYPE_INSTANCE)

#define tea_is_mask(T, n, m) (tea_get_mask(T, (n)) & (m))
#define tea_is_nonenil(T, n) (tea_get_type(T, (n)) <= TEA_TYPE_NONE)
#define tea_is_none(T, n) (tea_get_type(T, (n)) == TEA_TYPE_NONE)
#define tea_is_nil(T, n) (tea_get_type(T, (n)) == TEA_TYPE_NIL)
#define tea_is_bool(T, n) (tea_get_type(T, (n)) == TEA_TYPE_BOOL)
#define tea_is_number(T, n) (tea_get_type(T, (n)) == TEA_TYPE_NUMBER)
#define tea_is_pointer(T, n) (tea_get_type(T, (n)) == TEA_TYPE_NIL)
#define tea_is_range(T, n) (tea_get_type(T, (n)) == TEA_TYPE_RANGE)
#define tea_is_string(T, n) (tea_get_type(T, (n)) == TEA_TYPE_STRING)
#define tea_is_list(T, n) (tea_get_type(T, (n)) == TEA_TYPE_LIST)
#define tea_is_map(T, n) (tea_get_type(T, (n)) == TEA_TYPE_MAP)
#define tea_is_function(T, n) (tea_get_type(T, (n)) == TEA_TYPE_FUNCTION)
#define tea_is_instance(T, n) (tea_get_type(T, (n)) == TEA_TYPE_INSTANCE)

#endif