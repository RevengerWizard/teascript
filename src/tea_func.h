/*
** tea_func.h
** Teascript closures, functions and upvalues
*/

#ifndef TEA_FUNC_H
#define TEA_FUNC_H

#include "tea_object.h"

TEA_FUNC TeaONative* tea_func_new_native(TeaState* T, TeaNativeType type, TeaCFunction fn);

TEA_FUNC TeaOFunction* tea_func_new_function(TeaState* T, TeaFunctionType type, TeaOModule* module, int max_slots);
TEA_FUNC TeaOClosure* tea_func_new_closure(TeaState* T, TeaOFunction* function);

TEA_FUNC TeaOUpvalue* tea_func_new_upvalue(TeaState* T, TeaValue* slot);
TEA_FUNC TeaOUpvalue* tea_func_capture(TeaState* T, TeaValue* local);
TEA_FUNC void tea_func_close(TeaState* T, TeaValue* last);

#endif