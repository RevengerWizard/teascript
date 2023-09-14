/*
** tea_import.h
** Teascript import loading
*/

#ifndef TEA_IMPORT_H
#define TEA_IMPORT_H

#include "tea_state.h"
#include "tea_object.h"

TEA_FUNC void tea_imp_relative(TeaState* T, TeaOString* mod, TeaOString* path_name);
TEA_FUNC void tea_imp_logical(TeaState* T, TeaOString* name);

#endif