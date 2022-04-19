#ifndef TEA_TYPE_H
#define TEA_TYPE_H

#include "tea_value.h"
#include "tea_object.h"
#include "tea_vm.h"

void tea_define_file_methods(TeaVM* vm);
void tea_define_list_methods(TeaVM* vm);
void tea_define_string_methods(TeaVM* vm);

#endif