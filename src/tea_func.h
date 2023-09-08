/*
** tea_func.h
** Teascript closures, functions and upvalues
*/

#ifndef TEA_FUNC_H
#define TEA_FUNC_H

#include "tea_object.h"

TEA_FUNC TeaObjectNative* tea_func_new_native(TeaState* T, TeaNativeType type, TeaCFunction fn);

TEA_FUNC TeaObjectFunction* tea_func_new_function(TeaState* T, TeaFunctionType type, TeaObjectModule* module, int max_slots);
TEA_FUNC TeaObjectClosure* tea_func_new_closure(TeaState* T, TeaObjectFunction* function);

TEA_FUNC TeaObjectUpvalue* tea_func_new_upvalue(TeaState* T, TeaValue* slot);
TEA_FUNC TeaObjectUpvalue* tea_func_capture_upvalue(TeaState* T, TeaValue* local);
TEA_FUNC void tea_func_close_upvalues(TeaState* T, TeaValue* last);

#endif