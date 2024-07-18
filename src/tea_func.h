/*
** tea_func.h
** Function handling (prototypes, functions and upvalues)
*/

#ifndef _TEA_FUNC_H
#define _TEA_FUNC_H

#include "tea_obj.h"

/* Prototypes */
TEA_FUNC GCproto* tea_func_newproto(tea_State* T, int max_slots);
TEA_FUNC int tea_func_getline(GCproto* f, int instruction);

/* Upvalues */
TEA_FUNC GCupval* tea_func_finduv(tea_State* T, TValue* local);
TEA_FUNC void tea_func_closeuv(tea_State* T, TValue* last);

/* Functions (closures) */
TEA_FUNC GCfunc* tea_func_newC(tea_State* T, CFuncType type, tea_CFunction fn, int nargs, int nupvalues);
TEA_FUNC GCfunc* tea_func_newT(tea_State* T, GCproto* proto, GCmodule* module);
TEA_FUNC void tea_func_free(tea_State* T, GCfunc* fn);

#endif