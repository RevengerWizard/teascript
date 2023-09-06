/* 
** tea_dump.h
** Teascript bytecode saving
*/

#ifndef TEA_DUMP_H
#define TEA_DUMP_H

#include <stdio.h>

#include "tea.h"

#include "tea_object.h"

void tea_dump(TeaState* T, TeaObjectFunction* f, FILE* file);
TeaObjectClosure* tea_undump(TeaState* T, const char* name, FILE* file);

#endif