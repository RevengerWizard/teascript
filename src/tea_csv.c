#include "tea_module.h"

TeaValue tea_import_csv(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_CSV_MODULE, 3);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    return OBJECT_VAL(module);
}