#include "hashlib/utils.h"
#include "hashlib/sha256.h"
#include "hashlib/hmac.h"
#include "hashlib/bcrypt/bcrypt.h"

#include "tea_module.h"

TeaValue tea_import_hash(TeaVM* vm)
{
    TeaObjectString* name = tea_copy_string(vm->state, TEA_HASH_MODULE, 4);
    TeaObjectModule* module = tea_new_module(vm->state, name);

    return OBJECT_VAL(module);
}