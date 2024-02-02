/*
** tea_func.h
** Function handling (prototypes, functions and upvalues)
*/

#ifndef _TEA_FUNC_H
#define _TEA_FUNC_H

#include "tea_obj.h"

/* Functions (closures) */
TEA_FUNC GCfuncC* tea_func_newC(tea_State* T, CFuncType type, tea_CFunction fn, int nargs);
TEA_FUNC int tea_func_getline(GCproto* f, int instruction);
TEA_FUNC GCproto* tea_func_newproto(tea_State* T, ProtoType type, GCmodule* module, int max_slots);
TEA_FUNC GCfuncT* tea_func_newT(tea_State* T, GCproto* proto);

/* Upvalues */
TEA_FUNC GCupvalue* tea_func_new_upvalue(tea_State* T, Value* slot);
TEA_FUNC GCupvalue* tea_func_capture(tea_State* T, Value* local);
TEA_FUNC void tea_func_close(tea_State* T, Value* last);

#endif