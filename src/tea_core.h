/*
** tea_core.h
** Teascript core classes and functions
*/

#ifndef TEA_CORE_H
#define TEA_CORE_H

#include "tea_state.h"

#define TEA_FILE_CLASS "File"
void tea_open_file(TeaState* T);

#define TEA_LIST_CLASS "List"
void tea_open_list(TeaState* T);

#define TEA_MAP_CLASS "Map"
void tea_open_map(TeaState* T);

#define TEA_STRING_CLASS "String"
void tea_open_string(TeaState* T);

#define TEA_RANGE_CLASS "Range"
void tea_open_range(TeaState* T);

TEA_FUNC void tea_open_core(TeaState* T);

#endif