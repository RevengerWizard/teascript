#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tea_type.h"
#include "tea_native.h"

static TeaValue reverse_string(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    const char* string = AS_CSTRING(args[0]);

    int l = strlen(string);

    if(l == 0 || l == 1)
    {
        return OBJECT_VAL(AS_STRING(args[0]));
    }

    char* res = ALLOCATE(vm->state, char, l + 1);
    for(int i = 0; i < l; i++)
    {
        res[i] = string[l - i - 1];
    }
    res[l] = '\0';

    return OBJECT_VAL(tea_take_string(vm->state, res, l));
}

void tea_define_string_methods(TeaVM* vm)
{
    tea_native_function(vm, &vm->string_methods, "reverse", reverse_string);
}