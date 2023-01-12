// tea_core.h
// Teascript core classes and functions

#ifndef TEA_CORE_H
#define TEA_CORE_H

#include "tea_state.h"

#define TEA_FILE_CLASS "file"
void tea_open_file(TeaState* T);

#define TEA_LIST_CLASS "list"
void tea_open_list(TeaState* T);

#define TEA_MAP_CLASS "map"
void tea_open_map(TeaState* T);

#define TEA_STRING_CLASS "string"
void tea_open_string(TeaState* T);

#define TEA_RANGE_CLASS "range"
void tea_open_range(TeaState* T);

void tea_open_core(TeaState* T);

#endif