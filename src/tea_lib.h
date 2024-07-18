/*
** tea_lib.h
** Library function support
*/

#ifndef _TEA_LIB_H
#define _TEA_LIB_H

#include "tea_obj.h"

TEA_FUNC TValue* tea_lib_checkany(tea_State* T, int idx);
TEA_FUNC tea_Number tea_lib_checknumber(tea_State* T, int idx);
TEA_FUNC int32_t tea_lib_checkint(tea_State* T, int idx);
TEA_FUNC int32_t tea_lib_optint(tea_State* T, int idx, int32_t def);
TEA_FUNC GCstr* tea_lib_checkstr(tea_State* T, int idx);
TEA_FUNC GCstr* tea_lib_optstr(tea_State* T, int idx);
TEA_FUNC GClist* tea_lib_checklist(tea_State* T, int idx);
TEA_FUNC GCmap* tea_lib_checkmap(tea_State* T, int idx);
TEA_FUNC GCrange* tea_lib_checkrange(tea_State* T, int idx);
TEA_FUNC int tea_lib_checkopt(tea_State* T, int idx, int def, const char* lst);
TEA_FUNC int32_t tea_lib_checkintrange(tea_State* T, int idx, int32_t a, int32_t b);

TEA_FUNC void tea_lib_fileresult(tea_State* T, const char* fname);

#endif