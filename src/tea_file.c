#include "tea_type.h"
#include "tea_native.h"
#include "tea_memory.h"

static TeaValue write_file(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(write, 1);

    if(!IS_STRING(args[1]))
    {
        NATIVE_ERROR("write() argument must be a string.");
    }

    TeaObjectFile* file = AS_FILE(args[0]);
    TeaObjectString* string = AS_STRING(args[1]);

    if(strcmp(file->type, "r") == 0)
    {
        NATIVE_ERROR("File is not readable.");
    }

    int chars_wrote = fprintf(file->file, "%s", string->chars);
    fflush(file->file);

    return NUMBER_VAL(chars_wrote);
}

static TeaValue writeline_file(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(writeline, 1);

    if(!IS_STRING(args[1]))
    {
        NATIVE_ERROR("writeline() argument must be a string.");
    }

    TeaObjectFile* file = AS_FILE(args[0]);
    TeaObjectString* string = AS_STRING(args[1]);

    if(strcmp(file->type, "r") == 0)
    {
        NATIVE_ERROR("File is not readable.");
    }

    int chars_wrote = fprintf(file->file, "%s\n", string->chars);
    fflush(file->file);

    return NUMBER_VAL(chars_wrote);
}

static TeaValue read_file(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(read, 0);

    TeaObjectFile* file = AS_FILE(args[0]);
}

static TeaValue readline_file(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(readline, 0);

    return NULL_VAL;
}

static TeaValue seek_file(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    RANGE_ARG_COUNT(seek, 2, "1 or 2");

    return NULL_VAL;
}

void tea_define_file_methods(TeaVM* vm)
{
    //tea_native_function(vm, &vm->file_methods, "write", write_file);
    //tea_native_function(vm, &vm->file_methods, "writeline", writeline_file);
    //tea_native_function(vm, &vm->file_methods, "read", read_file);
    //tea_native_function(vm, &vm->file_methods, "readline", readline_file);
    //tea_native_function(vm, &vm->file_methods, "seek", seek_file);
}