/*
** tea_func.h
** Teascript closures, functions and upvalues
*/

#ifndef TEA_FUNC_H
#define TEA_FUNC_H

#include "tea_object.h"

TeaObjectNative* tea_func_new_native(TeaState* T, TeaNativeType type, TeaCFunction fn);
TeaObjectFunction* tea_func_new_function(TeaState* T, TeaFunctionType type, TeaObjectModule* module, int max_slots);
TeaObjectClosure* tea_func_new_closure(TeaState* T, TeaObjectFunction* function);

TeaObjectUpvalue* tea_func_new_upvalue(TeaState* T, TeaValue* slot);
TeaObjectUpvalue* tea_func_capture_upvalue(TeaState* T, TeaValue* local);
void tea_func_close_upvalues(TeaState* T, TeaValue* last);

#endif