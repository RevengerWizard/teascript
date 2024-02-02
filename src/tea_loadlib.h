/*
** tea_loadlib.h
** Dynamic library loader
*/

#ifndef _TEA_LOADLIB_H
#define _TEA_LOADLIB_H

#include "tea_state.h"

#define TEA_LL_SYM "tea_import_"

TEA_FUNC void tea_ll_unload(void* lib);
TEA_FUNC void* tea_ll_load(tea_State* T, const char* path);
TEA_FUNC tea_CFunction tea_ll_sym(tea_State* T, void* lib, const char* sym);

#endif