#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "tea_memory.h"
#include "tea_native.h"

static TeaValue print_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
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

static TeaValue input_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
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
    char* line = ALLOCATE(vm->state, char, current_size);

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
            line = GROW_ARRAY(vm->state, char, line, old_size, current_size);

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
        line = GROW_ARRAY(vm->state, char, line, current_size, length + 1);
    }

    line[length] = '\0';

    return OBJECT_VAL(tea_take_string(vm->state, line, length));
}

static TeaValue open_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(open, 2);

    if(!IS_STRING(args[0]) || !IS_STRING(args[1]))
    {
        NATIVE_ERROR("open() expects two strings.");
    }

    TeaObjectFile* file = tea_new_file(vm->state);
    file->path = AS_STRING(args[0])->chars;
    file->type = AS_STRING(args[1])->chars;
    file->is_open = true;

    return OBJECT_VAL(file);
}

static TeaValue assert_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(assert, 2);

    if(tea_is_falsey(args[0]))
    {
        NATIVE_ERROR("%s", AS_CSTRING(args[1]));
    }

    return EMPTY_VAL;
}

static TeaValue error_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(error, 1);
    
    NATIVE_ERROR("%s", AS_CSTRING(args[0]));

    return EMPTY_VAL;
}

static TeaValue type_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(type, 1);

    const char* type = tea_value_type(args[0]);

    return OBJECT_VAL(tea_copy_string(vm->state, type, (int)strlen(type)));
}

static TeaValue number_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(number, 1);

    if(IS_NUMBER(args[0]))
    {
        return args[0];
    }
    else if(IS_BOOL(args[0]))
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
            NATIVE_ERROR("Failed conversion.");
        }

        return NUMBER_VAL(number);
    }
}

static TeaValue int_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(int, 1);

    if(!IS_NUMBER(args[0]))
    {
        NATIVE_ERROR("int() takes a number as parameter.");
    }

    return NUMBER_VAL((int)(AS_NUMBER(args[0])));
}

static TeaValue string_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(string, 1);

    const char* string = tea_value_tostring(vm->state, args[0]);

    return OBJECT_VAL(tea_copy_string(vm->state, string, strlen(string)));
}

static TeaValue gc_native(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(gc, 0);

    tea_collect_garbage(vm);

    return EMPTY_VAL;
}

void tea_native_property(TeaVM* vm, TeaTable* table, const char* name, TeaValue value)
{
    TeaObjectString* property = tea_copy_string(vm->state, name, strlen(name));
    tea_table_set(vm->state, table, property, value);
}

void tea_native_function(TeaVM* vm, TeaTable* table, const char* name, TeaNativeFunction function)
{
    TeaObjectNative* native = tea_new_native(vm->state, function);
    TeaObjectString* method = tea_copy_string(vm->state, name, strlen(name));
    tea_table_set(vm->state, table, method, OBJECT_VAL(native));
}

void tea_define_natives(TeaVM* vm)
{
    tea_native_function(vm, &vm->globals, "print", print_native);
    tea_native_function(vm, &vm->globals, "input", input_native);
    tea_native_function(vm, &vm->globals, "open", open_native);
    tea_native_function(vm, &vm->globals, "assert", assert_native);
    tea_native_function(vm, &vm->globals, "error", error_native);
    tea_native_function(vm, &vm->globals, "type", type_native);
    tea_native_function(vm, &vm->globals, "gc", gc_native);

    tea_native_function(vm, &vm->globals, "number", number_native);
    tea_native_function(vm, &vm->globals, "int", int_native);
    //tea_native_function(vm, &vm->globals, "bool", bool_native);
    tea_native_function(vm, &vm->globals, "string", string_native);
    //tea_native_function(vm, &vm->globals, "list", list_native);

    //tea_native_function(vm, &vm->globals, "len", len_native);
}