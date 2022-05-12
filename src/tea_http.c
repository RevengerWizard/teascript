#include "tea_module.h"

TeaValue tea_import_http(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_HTTP_MODULE, 4);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    return OBJECT_VAL(module);
}