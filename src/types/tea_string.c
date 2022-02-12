#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types/tea_string.h"
#include "vm/tea_native.h"

static TeaValue reverse_string(int arg_count, TeaValue* args, bool* error)
{
    char* string = AS_CSTRING(args[0]);

    int l = strlen(string);

    if(l == 0 || l == 1)
    {
        return OBJECT_VAL(AS_STRING(args[0]));
    }

    return OBJECT_VAL(tea_take_string(strrev(string), l));
}

void tea_define_string_methods(TeaVM* vm)
{
    tea_native_function(vm, &vm->string_methods, "reverse", reverse_string);
}