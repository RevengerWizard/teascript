/*
** tea_import.h
** Import loader
*/

#ifndef _TEA_IMPORT_H
#define _TEA_IMPORT_H

#include "tea_def.h"
#include "tea_state.h"
#include "tea_obj.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if TEA_TARGET_WINDOWS
#define DIR_SEP '\\'
#define DIR_ALT_SEP '/'
#else
#define DIR_SEP '/'
#endif

#ifdef DIR_ALT_SEP
#define IS_DIR_SEP(c) ((c) == DIR_SEP || (c) == DIR_ALT_SEP)
#else
#define IS_DIR_SEP(c) (c == DIR_SEP)
#endif

#define TEA_LL_SYM "tea_import_"

TEA_FUNC GCstr* tea_imp_dirname(tea_State* T, char* path, int len);
TEA_FUNC bool tea_imp_resolvepath(char* directory, char* path, char* ret);
TEA_FUNC GCstr* tea_imp_getdir(tea_State* T, char* source);

TEA_FUNC void tea_imp_relative(tea_State* T, GCstr* mod, GCstr* path_name);
TEA_FUNC void tea_imp_logical(tea_State* T, GCstr* name);

#endif