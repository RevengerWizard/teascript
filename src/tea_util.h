/*
** tea_util.h
** Teascript utility functions
*/

#ifndef TEA_UTIL_H
#define TEA_UTIL_H

#include "tea_vm.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef _WIN32
#define DIR_SEPARATOR '\\'
#define DIR_ALT_SEPARATOR '/'
#define DIR_SEPARATOR_AS_STRING "\\"
#define DIR_SEPARATOR_STRLEN 1
#define PATH_DELIMITER ';'
#define PATH_DELIMITER_AS_STRING ";"
#define PATH_DELIMITER_STRLEN 1
#else
#define HAS_REALPATH
#define DIR_SEPARATOR '/'
#define DIR_SEPARATOR_AS_STRING "/"
#define DIR_SEPARATOR_STRLEN 1
#define PATH_DELIMITER ':'
#define PATH_DELIMITER_AS_STRING ":"
#define PATH_DELIMITER_STRLEN 1
#endif

#ifdef DIR_ALT_SEPARATOR
#define IS_DIR_SEPARATOR(c) ((c) == DIR_SEPARATOR || (c) == DIR_ALT_SEPARATOR)
#else
#define IS_DIR_SEPARATOR(c) (c == DIR_SEPARATOR)
#endif

TEA_FUNC char* tea_util_read_file(TeaState* T, const char* path);
TEA_FUNC TeaOString* tea_util_dirname(TeaState* T, char* path, int len);
TEA_FUNC bool tea_util_resolve_path(char* directory, char* path, char* ret);
TEA_FUNC TeaOString* tea_util_get_directory(TeaState* T, char* source);

#endif