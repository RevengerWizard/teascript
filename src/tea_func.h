/*
** tea_func.h
** Function handling (prototypes, functions and upvalues)
*/

#ifndef _TEA_FUNC_H
#define _TEA_FUNC_H

#include "tea_obj.h"

/* Prototypes */
TEA_FUNC void TEA_FASTCALL tea_func_freeproto(tea_State* T, GCproto* pt);

/* Upvalues */
TEA_FUNC GCupval* tea_func_finduv(tea_State* T, TValue* local);
TEA_FUNC void tea_func_closeuv(tea_State* T, TValue* last);
TEA_FUNC void TEA_FASTCALL tea_func_freeuv(tea_State* T, GCupval* uv);

/* Functions (closures) */
TEA_FUNC GCfunc* tea_func_newC(tea_State* T, CFuncType type, tea_CFunction fn, int nupvalues, int nargs, int nopts);
TEA_FUNC GCfunc* tea_func_newT_empty(tea_State* T, GCproto* pt, GCmodule* module);
TEA_FUNC GCfunc* tea_func_newT(tea_State* T, GCproto* pt, GCfuncT* parent);
TEA_FUNC void TEA_FASTCALL tea_func_free(tea_State* T, GCfunc* fn);

#endif