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

    size_t current_position = ftell(file->file);

    // Calculate file size
    fseek(file->file, 0L, SEEK_END);
    size_t file_size = ftell(file->file);
    fseek(file->file, current_position, SEEK_SET);

    char* buffer = ALLOCATE(vm->state, char, file_size + 1);
    if (buffer == NULL) 
    {
        NATIVE_ERROR("Not enough memory to read \"%s\".\n", file->path);
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file->file);
    if(bytes_read < file_size && !feof(file->file)) 
    {
        FREE_ARRAY(vm->state, char, buffer, file_size + 1);
        NATIVE_ERROR("Could not read file \"%s\".\n", file->path);
    }

    if(bytes_read != file_size)
    {
        buffer = GROW_ARRAY(vm->state, char, buffer, file_size + 1, bytes_read + 1);
    }

    buffer[bytes_read] = '\0';
    return OBJECT_VAL(tea_take_string(vm->state, buffer, bytes_read));
}

static TeaValue readline_file(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(readline, 0);

    char line[4096];

    TeaObjectFile* file = AS_FILE(args[0]);
    if(fgets(line, 4096, file->file) != NULL) 
    {
        int line_length = strlen(line);
        // Remove newline char
        if(line[line_length - 1] == '\n') 
        {
            line_length--;
            line[line_length] = '\0';
        }
        return OBJECT_VAL(tea_copy_string(vm->state, line, line_length));
    }

    return NULL_VAL;
}

static TeaValue seek_file(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    RANGE_ARG_COUNT(seek, 2, "1 or 2");

    int seek_type = SEEK_SET;

    if(arg_count == 2) 
    {
        if(!IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) 
        {
            NATIVE_ERROR("seek() arguments must be numbers");
        }

        int seek_type_num = AS_NUMBER(args[2]);

        switch(seek_type_num) 
        {
            case 0:
                seek_type = SEEK_SET;
                break;
            case 1:
                seek_type = SEEK_CUR;
                break;
            case 2:
                seek_type = SEEK_END;
                break;
            default:
                seek_type = SEEK_SET;
                break;
        }
    }

    if(!IS_NUMBER(args[1])) 
    {
        NATIVE_ERROR("seek() argument must be a number");
        return EMPTY_VAL;
    }

    int offset = AS_NUMBER(args[1]);
    TeaObjectFile* file = AS_FILE(args[0]);

    if(offset != 0 && !strstr(file->type, "b")) 
    {
        NATIVE_ERROR("seek() may not have non-zero offset if file is opened in text mode.");
    }

    fseek(file->file, offset, seek_type);

    return NULL_VAL;
}

void tea_define_file_methods(TeaVM* vm)
{
    tea_native_function(vm, &vm->file_methods, "write", write_file);
    tea_native_function(vm, &vm->file_methods, "writeline", writeline_file);
    tea_native_function(vm, &vm->file_methods, "read", read_file);
    tea_native_function(vm, &vm->file_methods, "readline", readline_file);
    tea_native_function(vm, &vm->file_methods, "seek", seek_file);
}