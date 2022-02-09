#ifndef TEA_PARSER_H
#define TEA_PARSER_H

#include "vm/tea_object.h"
#include "vm/tea_vm.h"

TeaObjectFunction* tea_compile(const char* source);
void tea_mark_compiler_roots();

#endif