/*
** tea_loadlib.h
** Dynamic library loader for Teascript
*/

#ifndef TEA_LOADLIB_H
#define TEA_LOADLIB_H

#include "tea_state.h"

#define TEA_POF "tea_import_"

void tea_ll_unload(void* lib);
void* tea_ll_load(TeaState* T, const char* path);
TeaCFunction tea_ll_sym(TeaState* T, void* lib, const char* sym);

#endif