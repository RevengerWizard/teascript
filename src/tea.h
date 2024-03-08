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

#define TEA_SIGNATURE "\x1bTea"

#define TEA_MIN_STACK 20

typedef struct tea_State tea_State;

typedef void (*tea_CFunction)(tea_State* T);

typedef const char* (*tea_Reader)(tea_State* T, void* ud, size_t* sz);

typedef int (*tea_Writer)(tea_State* T, void* ud, const void* p, size_t sz);

typedef void* (*tea_Alloc)(void* ud, void* ptr, size_t osize, size_t nsize);

typedef struct tea_Reg
{
    const char* name;
    tea_CFunction fn;
    int nargs;
} tea_Reg;

typedef tea_Reg tea_Module;

typedef struct tea_Class
{
    const char* name;
    const char* type;
    tea_CFunction fn;
    int nargs;
} tea_Class;

typedef tea_Class tea_Instance;

#define TEA_MASK_NONE       (1 << TEA_TYPE_NONE)
#define TEA_MASK_NULL       (1 << TEA_TYPE_NULL)
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
#define TEA_MASK_FILE       (1 << TEA_TYPE_FILE)

#define TEA_VARARGS (-1)

enum
{
    TEA_OK,
    TEA_ERROR_SYNTAX,
    TEA_ERROR_RUNTIME,
    TEA_ERROR_MEMORY,
    TEA_ERROR_FILE,
    TEA_ERROR_ERROR
};

enum
{
    TEA_TYPE_NONE,
    TEA_TYPE_NULL,
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
    TEA_TYPE_FILE,
};

TEA_API tea_State* tea_new_state(tea_Alloc allocf, void* ud);
TEA_API void tea_close(tea_State* T);
TEA_API void tea_set_argv(tea_State* T, int argc, char** argv, int argf);
TEA_API void tea_set_repl(tea_State* T, bool b);

TEA_API tea_CFunction tea_atpanic(tea_State* T, tea_CFunction panicf);

TEA_API tea_Alloc tea_get_allocf(tea_State* T, void** ud);
TEA_API void tea_set_allocf(tea_State* T, tea_Alloc f, void* ud);

TEA_API int tea_get_top(tea_State* T);
TEA_API void tea_set_top(tea_State* T, int index);
TEA_API void tea_push_value(tea_State* T, int index);
TEA_API void tea_remove(tea_State* T, int index);
TEA_API void tea_insert(tea_State* T, int index);
TEA_API void tea_replace(tea_State* T, int index);
TEA_API void tea_copy(tea_State* T, int from_index, int to_index);

TEA_API const char* tea_typeof(tea_State* T, int index);

TEA_API int tea_get_mask(tea_State* T, int index);
TEA_API int tea_get_type(tea_State* T, int index);
TEA_API bool tea_get_bool(tea_State* T, int index);
TEA_API double tea_get_number(tea_State* T, int index);
TEA_API const void* tea_get_pointer(tea_State* T, int index);
TEA_API void tea_get_range(tea_State* T, int index, double* start, double* end, double* step);
TEA_API const char* tea_get_lstring(tea_State* T, int index, int* len);

TEA_API bool tea_is_object(tea_State* T, int index);
TEA_API bool tea_is_cfunction(tea_State* T, int index);

TEA_API bool tea_to_bool(tea_State* T, int index);
TEA_API double tea_to_numberx(tea_State* T, int index, bool* is_num);
TEA_API const void* tea_to_pointer(tea_State* T, int index);
TEA_API const char* tea_to_lstring(tea_State* T, int index, int* len);
TEA_API tea_CFunction tea_to_cfunction(tea_State* T, int index);

TEA_API bool tea_equal(tea_State* T, int index1, int index2);
TEA_API bool tea_rawequal(tea_State* T, int index1, int index2);

TEA_API void tea_concat(tea_State* T);

TEA_API void tea_pop(tea_State* T, int n);

TEA_API void tea_push_null(tea_State* T);
TEA_API void tea_push_true(tea_State* T);
TEA_API void tea_push_false(tea_State* T);
TEA_API void tea_push_bool(tea_State* T, bool b);
TEA_API void tea_push_number(tea_State* T, double n);
TEA_API void tea_push_pointer(tea_State* T, void* p);
TEA_API const char* tea_push_lstring(tea_State* T, const char* s, int len);
TEA_API const char* tea_push_string(tea_State* T, const char* s);
TEA_API const char* tea_push_fstring(tea_State* T, const char* fmt, ...);
TEA_API const char* tea_push_vfstring(tea_State* T, const char* fmt, va_list args);
TEA_API void tea_push_range(tea_State* T, double start, double end, double step);
TEA_API void tea_push_cfunction(tea_State* T, tea_CFunction fn, int nargs);

TEA_API void tea_new_list(tea_State* T);
TEA_API void tea_new_map(tea_State* T);

TEA_API void tea_create_class(tea_State* T, const char* name, const tea_Class* klass);
TEA_API void tea_create_module(tea_State* T, const char* name, const tea_Module* module);

TEA_API int tea_len(tea_State* T, int index);

TEA_API void tea_add_item(tea_State* T, int list);
TEA_API bool tea_get_item(tea_State* T, int list, int index);
TEA_API void tea_set_item(tea_State* T, int list, int index);

TEA_API bool tea_get_field(tea_State* T, int obj);
TEA_API void tea_set_field(tea_State* T, int obj);

TEA_API bool tea_get_key(tea_State* T, int obj, const char* key);
TEA_API void tea_set_key(tea_State* T, int obj, const char* key);

TEA_API bool tea_get_global(tea_State* T, const char* name);
TEA_API void tea_set_global(tea_State* T, const char* name);
TEA_API void tea_set_funcs(tea_State* T, const tea_Reg* reg);

TEA_API bool tea_has_module(tea_State* T, const char* module);

TEA_API bool tea_test_stack(tea_State* T, int size);
TEA_API void tea_check_stack(tea_State* T, int size, const char* msg);

TEA_API void tea_check_type(tea_State* T, int index, int type);
TEA_API void tea_check_any(tea_State* T, int index);
TEA_API bool tea_check_bool(tea_State* T, int index);
TEA_API double tea_check_number(tea_State* T, int index);
TEA_API const void* tea_check_pointer(tea_State* T, int index);
TEA_API void tea_check_range(tea_State* T, int index, double* start, double* end, double* step);
TEA_API const char* tea_check_lstring(tea_State* T, int index, int* len);
TEA_API tea_CFunction tea_check_cfunction(tea_State* T, int index);
TEA_API int tea_check_option(tea_State* T, int index, const char* def, const char* const options[]);

TEA_API void tea_opt_any(tea_State* T, int index);
TEA_API bool tea_opt_bool(tea_State* T, int index, bool def);
TEA_API double tea_opt_number(tea_State* T, int index, double def);
TEA_API const void* tea_opt_pointer(tea_State* T, int index, void* def);
TEA_API const char* tea_opt_lstring(tea_State* T, int index, const char* def, int* len);

TEA_API int tea_gc(tea_State* T);

TEA_API void tea_call(tea_State* T, int n);
TEA_API int tea_pcall(tea_State* T, int n);

TEA_API int tea_loadx(tea_State* T, tea_Reader reader, void* data, const char* name, const char* mode);
TEA_API int tea_dump(tea_State* T, tea_Writer writer, void* data);

TEA_API int tea_load_pathx(tea_State* T, const char* filename, const char* name, const char* mode);
TEA_API int tea_load_filex(tea_State* T, const char* filename, const char* mode);
TEA_API int tea_load_bufferx(tea_State* T, const char* buffer, size_t size, const char* name, const char* mode);
TEA_API int tea_load_string(tea_State* T, const char* s);

TEA_API void tea_error(tea_State* T, const char* fmt, ...);

#define tea_open()  tea_new_state(NULL, NULL)

#define tea_get_string(T, index) (tea_get_lstring(T, (index), NULL))
#define tea_to_number(T, index) (tea_to_numberx(T, (index), NULL))
#define tea_to_string(T, index) (tea_to_lstring(T, (index), NULL))

#define tea_push_literal(T, s)  (tea_push_lstring(T, "" s, (sizeof(s)/sizeof(char))-1))

#define tea_opt_string(T, index, def) (tea_opt_lstring(T, (index), (def), NULL))

#define tea_check_string(T, index) (tea_check_lstring(T, (index), NULL))
#define tea_check_list(T, index) (tea_check_type(T, index, TEA_TYPE_LIST))
#define tea_check_function(T, index) (tea_check_type(T, index, TEA_TYPE_FUNCTION))
#define tea_check_map(T, index) (tea_check_type(T, index, TEA_TYPE_MAP))
#define tea_check_file(T, index) (tea_check_type(T, index, TEA_TYPE_FILE))

#define tea_check_args(T, cond, msg, ...) if(cond) tea_error(T, (msg), __VA_ARGS__)

#define tea_load(T, reader, data, name) (tea_loadx(T, reader, data, name, NULL))
#define tea_load_path(T, filename, name) (tea_load_pathx(T, filename, name, NULL))
#define tea_load_file(T, filename) (tea_load_filex(T, filename, NULL))
#define tea_load_buffer(T, buffer, size, name) (tea_load_bufferx(T, buffer, size, name, NULL))

#define tea_register(T, n, f, args) (tea_push_cfunction(T, (f), (args)), tea_set_global(T, (n)))

#define tea_is_mask(T, n, m) (tea_get_mask(T, n) & (m))
#define tea_is_nonenull(T, n) (tea_get_type(T, (n)) <= TEA_TYPE_NONE)
#define tea_is_none(T, n) (tea_get_type(T, (n)) == TEA_TYPE_NONE)
#define tea_is_null(T, n) (tea_get_type(T, (n)) == TEA_TYPE_NULL)
#define tea_is_bool(T, n) (tea_get_type(T, (n)) == TEA_TYPE_BOOL)
#define tea_is_number(T, n) (tea_get_type(T, (n)) == TEA_TYPE_NUMBER)
#define tea_is_pointer(T, n) (tea_get_type(T, (n)) == TEA_TYPE_NULL)
#define tea_is_range(T, n) (tea_get_type(T, (n)) == TEA_TYPE_RANGE)
#define tea_is_string(T, n) (tea_get_type(T, (n)) == TEA_TYPE_STRING)
#define tea_is_list(T, n) (tea_get_type(T, (n)) == TEA_TYPE_LIST)
#define tea_is_map(T, n) (tea_get_type(T, (n)) == TEA_TYPE_MAP)
#define tea_is_function(T, n) (tea_get_type(T, (n)) == TEA_TYPE_FUNCTION)
#define tea_is_instance(T, n) (tea_get_type(T, (n)) == TEA_TYPE_INSTANCE)
#define tea_is_file(T, n) (tea_get_type(T, (n)) == TEA_TYPE_FILE)

#endif