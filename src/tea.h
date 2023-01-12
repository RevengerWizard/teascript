// tea.h
// Teascript C API

#ifndef TEA_H
#define TEA_H

#include <stdarg.h>
#include <stddef.h>

#include "tea_config.h"

#define TEA_REPOSITORY "https://github.com/RevengerWizard/teascript"

#define TEA_VERSION_MAJOR 0
#define TEA_VERSION_MINOR 0
#define TEA_VERSION_PATCH 0

#define TEA_VERSION_NUMBER (TEA_VERSION_MAJOR * 10000 + TEA_VERSION_MINOR * 100 + TEA_VERSION_PATCH)

#define TEA_VERSION str(TEA_VERSION_MAJOR) "." str(TEA_VERSION_MINOR) "." str(TEA_VERSION_PATCH)

#define TEA_SIGNATURE "\x1bTea"

#define TEA_BYTECODE_FORMAT 0

#define TEA_MIN_SLOTS 40

typedef struct TeaState TeaState;

typedef void (*TeaCFunction)(TeaState* T);

typedef void* (*TeaReallocateFn)(void* memory, size_t new_size);

typedef const char* (*TeaResolveModuleFn)(TeaState* T, const char* importer, const char* name);

typedef struct TeaReg
{
    const char* name;
    TeaCFunction fn;
} TeaReg;

typedef TeaReg TeaModule;

typedef struct TeaClass
{
    const char* name;
    const char* type;
    TeaCFunction fn;
} TeaClass;

typedef enum TeaInterpretResult
{
    TEA_OK,
    TEA_COMPILE_ERROR,
    TEA_RUNTIME_ERROR
} TeaInterpretResult;

typedef enum
{
    TEA_TYPE_UNKNOWN,
    TEA_TYPE_NULL,
    TEA_TYPE_NUMBER,
    TEA_TYPE_BOOL,
    TEA_TYPE_RANGE,
    TEA_TYPE_LIST,
    TEA_TYPE_MAP,
    TEA_TYPE_USERDATA,
    TEA_TYPE_CLASS,
    TEA_TYPE_INSTANCE,
    TEA_TYPE_MODULE,
    TEA_TYPE_FUNCTION,
    TEA_TYPE_STRING,
    TEA_TYPE_FILE
} TeaType;

TEA_API int tea_version();

TEA_API TeaState* tea_new_state(TeaState* T, TeaReallocateFn* realloc_fn);
TEA_API TeaState* tea_open();
TEA_API void tea_close(TeaState* T);
TEA_API void tea_set_argv(TeaState* T, int argc, const char** argv);
TEA_API const char** tea_get_argv(TeaState* T, int* argc);

TEA_API void tea_set_repl(TeaState* T, int b);

TEA_API int tea_get_top(TeaState* T);

TEA_API int tea_type(TeaState* T, int index);
TEA_API const char* tea_type_name(TeaState* T, int index);

TEA_API double tea_get_number(TeaState* T, int index);
#define tea_get_int(T, index) ((int)(tea_get_number(T, (index))))
TEA_API int tea_get_bool(TeaState* T, int index);
TEA_API void tea_get_range(TeaState* T, int index, double* start, double* end, double* step);
TEA_API const char* tea_get_lstring(TeaState* T, int index, int* len);
#define tea_get_string(T, index) (tea_get_lstring(T, (index), NULL))
TEA_API void* tea_get_userdata(TeaState* T, int index);

TEA_API int tea_falsey(TeaState* T, int index);
#define tea_truthy(T, index) (!tea_falsey(T, (index)))
TEA_API double tea_to_numberx(TeaState* T, int index, int* is_num);
#define tea_to_number(T, index) tea_to_numberx(T, (index), NULL)
TEA_API const char* tea_to_lstring(TeaState* T, int index, int* len);
#define tea_to_string(T, index) tea_to_lstring(T, (index), NULL)

TEA_API int tea_equals(TeaState* T, int index1, int index2);

TEA_API void tea_pop(TeaState* T, int n);

TEA_API void tea_push_value(TeaState* T, int index);
TEA_API void tea_push_null(TeaState* T);
TEA_API void tea_push_bool(TeaState* T, int b);
TEA_API void tea_push_number(TeaState* T, double n);
#define tea_push_int(T, n) tea_push_number(T, (double)(n))
TEA_API const char* tea_push_lstring(TeaState* T, const char* s, int len);
TEA_API const char* tea_push_string(TeaState* T, const char* s);
TEA_API const char* tea_push_fstring(TeaState* T, const char* fmt, ...);

TEA_API void tea_push_range(TeaState* T, double start, double end, double step);
TEA_API void tea_push_list(TeaState* T);
TEA_API void tea_push_map(TeaState* T);
TEA_API void tea_push_cfunction(TeaState* T, TeaCFunction fn);
TEA_API void tea_push_userdata(TeaState* T, void* data);

TEA_API void tea_create_class(TeaState* T, const char* name, const TeaClass* class);
TEA_API void tea_create_module(TeaState* T, const char* name, const TeaModule* module);
TEA_API void* tea_create_userdata(TeaState* T, size_t size);

TEA_API int tea_len(TeaState* T, int index);

TEA_API void tea_add_item(TeaState* T, int list);
TEA_API void tea_get_item(TeaState* T, int list, int index);
TEA_API void tea_set_item(TeaState* T, int list, int index);

TEA_API void tea_get_field(TeaState* T, int map);
TEA_API void tea_set_field(TeaState* T, int map);

TEA_API void tea_get_key(TeaState* T, int map, const char* key);
TEA_API void tea_set_key(TeaState* T, int map, const char* key);

TEA_API int tea_get_global(TeaState* T, const char* name);
TEA_API void tea_set_global(TeaState* T, const char* name);
TEA_API void tea_set_funcs(TeaState* T, const TeaReg* reg);

TEA_API void tea_get_variable(TeaState* T, const char* module, const char* name);
TEA_API void tea_set_variable(TeaState* T, const char* module, const char* name);
TEA_API int tea_has_module(TeaState* T, const char* module, const char* name);

TEA_API int tea_check_bool(TeaState* T, int index);
TEA_API void tea_check_range(TeaState* T, int index, double* start, double* end, double* step);
TEA_API double tea_check_number(TeaState* T, int index);
#define tea_check_int(T, index) ((int)(tea_check_number(T, (index))))
TEA_API const char* tea_check_lstring(TeaState* T, int index, int* len);
TEA_API const char* tea_check_string(TeaState* T, int index);
#define tea_check_string(T, index) tea_check_lstring(T, (index), NULL)

TEA_API void tea_check_list(TeaState* T, int index);
TEA_API void tea_check_map(TeaState* T, int index);
TEA_API void tea_check_file(TeaState* T, int index);

#define tea_check_args(T, cond, msg, ...) if(cond) tea_error(T, (msg), __VA_ARGS__)
#define tea_ensure_min_args(T, count, n) tea_check_args(T, ((count) < n), "Expected %d argument, got %d", (n), (count))
#define tea_ensure_max_args(T, count, n) tea_check_args(T, ((count) > n), "Expected %d argument, got %d", (n), (count))

TEA_API void tea_collect_garbage(TeaState* T);
TEA_API TeaInterpretResult tea_interpret(TeaState* T, const char* module_name, const char* source);
TEA_API TeaInterpretResult tea_call(TeaState* T, int n);

TEA_API void tea_error(TeaState* T, const char* fmt, ...);

#define tea_register(T, n, f) (tea_push_cfunction(T, (f)), tea_set_global(T, (n)))

#define tea_write_string(s, l)   (fwrite((s), sizeof(char), (l), stdout))
#define tea_write_lstring(s)   (fwrite((s), sizeof(char), strlen((s)), stdout))
#define tea_write_line()    (tea_write_string("\n", 1), fflush(stdout))
#define tea_write_version() (tea_write_lstring("teascript v" TEA_VERSION), tea_write_line())

#define tea_is_unknown(T, n) (tea_type(T, (n)) == TEA_TYPE_UNKNOWN)
#define tea_is_null(T, n) (tea_type(T, (n)) == TEA_TYPE_NULL)
#define tea_is_number(T, n) (tea_type(T, (n)) == TEA_TYPE_NUMBER)
#define tea_is_bool(T, n) (tea_type(T, (n)) == TEA_TYPE_BOOL)
#define tea_is_range(T, n) (tea_type(T, (n)) == TEA_TYPE_RANGE)
#define tea_is_string(T, n) (tea_type(T, (n)) == TEA_TYPE_STRING)
#define tea_is_list(T, n) (tea_type(T, (n)) == TEA_TYPE_LIST)
#define tea_is_map(T, n) (tea_type(T, (n)) == TEA_TYPE_MAP)
#define tea_is_file(T, n) (tea_type(T, (n)) == TEA_TYPE_FILE)

#endif