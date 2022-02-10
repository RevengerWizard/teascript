#ifndef TEA_MODULE_H
#define TEA_MODULE_H

#include "vm/tea_vm.h"

#include "modules/tea_math.h"
#include "modules/tea_time.h"

typedef TeaValue (*TeaNativeModule)(TeaVM* vm);

typedef struct
{
    char* name;
    int length;
    TeaNativeModule module;
} TeaNativeModules;

TeaValue tea_import_native_module(TeaVM* vm, int index);

int tea_find_native_module(char* name, int length);

#endif