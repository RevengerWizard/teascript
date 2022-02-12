#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory/tea_memory.h"
#include "vm/tea_native.h"

static TeaValue print_native(int arg_count, TeaValue* args, bool* error)
{
    if(arg_count == 0)
    {
        printf("\n");

        return EMPTY_VAL;
    }

    for(int i = 0; i < arg_count; i++)
    {
        tea_print_value(args[i]);
        printf("\t");
    }

    printf("\n");

    return EMPTY_VAL;
}

static TeaValue input_native(int arg_count, TeaValue* args, bool* error)
{
    RANGE_ARG_COUNT(input, 1, "0 or 1");

    if(arg_count != 0) 
    {
        TeaValue prompt = args[0];
        if(!IS_STRING(prompt)) 
        {
            NATIVE_ERROR("input() only takes a string argument");
        }

        printf("%s", AS_CSTRING(prompt));
    }

    uint64_t current_size = 128;
    char* line = ALLOCATE(char, current_size);

    if(line == NULL) 
    {
        NATIVE_ERROR("Memory error on input()!");
    }

    int c = EOF;
    uint64_t length = 0;
    while((c = getchar()) != '\n' && c != EOF) 
    {
        line[length++] = (char) c;

        if(length + 1 == current_size) 
        {
            int old_size = current_size;
            current_size = GROW_CAPACITY(current_size);
            line = GROW_ARRAY(char, line, old_size, current_size);

            if(line == NULL) 
            {
                printf("Unable to allocate memory\n");
                exit(71);
            }
        }
    }

    // If length has changed, shrink
    if(length != current_size) 
    {
        line = GROW_ARRAY(char, line, current_size, length + 1);
    }

    line[length] = '\0';

    return OBJECT_VAL(tea_take_string(line, length));
}

static TeaValue assert_native(int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(assert, 2);

    if(tea_is_falsey(args[0]))
    {
        NATIVE_ERROR("%s", AS_CSTRING(args[1]));
    }

    return EMPTY_VAL;
}

static TeaValue error_native(int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(error, 1);
    NATIVE_ERROR("%s", AS_CSTRING(args[0]));

    return EMPTY_VAL;
}

static TeaValue type_native(int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(type, 1);

    const char* type = tea_value_type(args[0]);

    return OBJECT_VAL(tea_copy_string(type, (int)strlen(type)));
}

static TeaValue number_native(int arg_count, TeaValue* args, bool* error)
{
    if(IS_BOOL(args[0]))
    {
        return AS_BOOL(args[0]) ? NUMBER_VAL(1) : NUMBER_VAL(0);
    }
    else if(IS_STRING(args[0]))
    {
        char* n = AS_CSTRING(args[0]);
        char* end;
        errno = 0;

        double number = strtod(n, &end);

        if(errno != 0 || *end != '\0')
        {
            NATIVE_ERROR("Failed conversion");
        }

        return NUMBER_VAL(number);
    }
}

void tea_native_property(TeaVM* vm, TeaTable* table, const char* name, TeaValue value)
{
    TeaObjectString* property = tea_copy_string(name, strlen(name));
    tea_table_set(table, property, value);
}

void tea_native_function(TeaVM* vm, TeaTable* table, const char* name, TeaNativeFunction function)
{
    TeaObjectNative* native = tea_new_native(function);
    TeaObjectString* method = tea_copy_string(name, strlen(name));
    tea_table_set(table, method, OBJECT_VAL(native));
}

void tea_define_natives(TeaVM* vm)
{
    tea_native_function(vm, &vm->globals, "print", print_native);
    tea_native_function(vm, &vm->globals, "input", input_native);
    tea_native_function(vm, &vm->globals, "assert", assert_native);
    tea_native_function(vm, &vm->globals, "error", error_native);
    tea_native_function(vm, &vm->globals, "type", type_native);

    tea_native_function(vm, &vm->globals, "number", number_native);
    //tea_native_function(vm, &vm->globals, "bool", bool_native);
    //tea_native_function(vm, &vm->globals, "string", string_native);
    //tea_native_function(vm, &vm->globals, "list", list_native);

    //tea_native_function(vm, &vm->globals, "len", len_native);
}