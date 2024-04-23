/*
** tea_lib.h
** Library function support
*/

#ifndef _TEA_LIB_H
#define _TEA_LIB_H

#include "tea_obj.h"

TEA_FUNC TValue* tea_lib_checkany(tea_State* T, int index);
TEA_FUNC tea_Number tea_lib_checknumber(tea_State* T, int index);
TEA_FUNC GCstr* tea_lib_checkstr(tea_State* T, int index);
TEA_FUNC GClist* tea_lib_checklist(tea_State* T, int index);
TEA_FUNC GCmap* tea_lib_checkmap(tea_State* T, int index);
TEA_FUNC GCrange* tea_lib_checkrange(tea_State* T, int index);

#endif