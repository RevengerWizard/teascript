#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "tea_core.h"
#include "tea_utf.h"

// File
static TeaValue write_file(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "write() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[1]))
    {
        tea_runtime_error(vm, "write() argument must be a string");
        return EMPTY_VAL;
    }

    TeaObjectFile* file = AS_FILE(args[0]);
    TeaObjectString* string = AS_STRING(args[1]);

    if(strcmp(file->type, "r") == 0)
    {
        tea_runtime_error(vm, "File is not readable");
        return EMPTY_VAL;
    }

    int chars_wrote = fprintf(file->file, "%s", string->chars);
    fflush(file->file);

    return NUMBER_VAL(chars_wrote);
}

static TeaValue writeline_file(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "floor() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[1]))
    {
        tea_runtime_error(vm, "writeline() argument must be a string");
        return EMPTY_VAL;
    }

    TeaObjectFile* file = AS_FILE(args[0]);
    TeaObjectString* string = AS_STRING(args[1]);

    if(strcmp(file->type, "r") == 0)
    {
        tea_runtime_error(vm, "File is not readable");
        return EMPTY_VAL;
    }

    int chars_wrote = fprintf(file->file, "%s\n", string->chars);
    fflush(file->file);

    return NUMBER_VAL(chars_wrote);
}

static TeaValue read_file(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "read() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }
    
    TeaObjectFile* file = AS_FILE(args[0]);

    size_t current_position = ftell(file->file);

    // Calculate file size
    fseek(file->file, 0L, SEEK_END);
    size_t file_size = ftell(file->file);
    fseek(file->file, current_position, SEEK_SET);

    char* buffer = ALLOCATE(vm->state, char, file_size + 1);
    if (buffer == NULL) 
    {
        tea_runtime_error(vm, "Not enough memory to read \"%s\".\n", file->path);
        return EMPTY_VAL;
    }

    size_t bytes_read = fread(buffer, sizeof(char), file_size, file->file);
    if(bytes_read < file_size && !feof(file->file)) 
    {
        FREE_ARRAY(vm->state, char, buffer, file_size + 1);
        tea_runtime_error(vm, "Could not read file \"%s\".\n", file->path);
        return EMPTY_VAL;
    }

    if(bytes_read != file_size)
    {
        buffer = GROW_ARRAY(vm->state, char, buffer, file_size + 1, bytes_read + 1);
    }

    buffer[bytes_read] = '\0';
    return OBJECT_VAL(tea_take_string(vm->state, buffer, bytes_read));
}

static TeaValue readline_file(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "readline() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

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

static TeaValue seek_file(TeaVM* vm, int count, TeaValue* args)
{
    if(count < 1 || count > 2)
    {
        tea_runtime_error(vm, "seek() expected either 1 or 2 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    int seek_type = SEEK_SET;

    if(count == 2) 
    {
        if(!IS_NUMBER(args[1]) || !IS_NUMBER(args[2])) 
        {
            tea_runtime_error(vm, "seek() arguments must be numbers");
            return EMPTY_VAL;
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
        tea_runtime_error(vm, "seek() argument must be a number");
        return EMPTY_VAL;
    }

    int offset = AS_NUMBER(args[1]);
    TeaObjectFile* file = AS_FILE(args[0]);

    if(offset != 0 && !strstr(file->type, "b")) 
    {
        tea_runtime_error(vm, "seek() may not have non-zero offset if file is opened in text mode");
        return EMPTY_VAL;
    }

    fseek(file->file, offset, seek_type);

    return NULL_VAL;
}

// String
static TeaValue reverse_string(TeaVM* vm, int count, TeaValue* args)
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

// List
static TeaValue add_list(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "add() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(args[0]);
    if(IS_LIST(args[1]) && AS_LIST(args[1]) == list)
    {
        tea_runtime_error(vm, "Cannot add list into itself");
        return EMPTY_VAL;
    }
    
    tea_write_value_array(vm->state, &list->items, args[1]);

    return EMPTY_VAL;
}

static TeaValue remove_list(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "remove() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(args[0]);
    TeaValue remove = args[1];
    bool found = false;

    if(list->items.count == 0) 
    {
        return EMPTY_VAL;
    }

    if(list->items.count > 1) 
    {
        for(int i = 0; i < list->items.count - 1; i++) 
        {
            if(!found && tea_values_equal(remove, list->items.values[i])) 
            {
                found = true;
            }

            // If we have found the value, shuffle the array
            if(found) 
            {
                list->items.values[i] = list->items.values[i + 1];
            }
        }

        // Check if it's the last element
        if(!found && tea_values_equal(remove, list->items.values[list->items.count - 1])) 
        {
            found = true;
        }
    } 
    else 
    {
        if(tea_values_equal(remove, list->items.values[0])) 
        {
            found = true;
        }
    }

    if(found) 
    {
        list->items.count--;
        return EMPTY_VAL;
    }

    tea_runtime_error(vm, "Value does not exist within the list");
    return EMPTY_VAL;
}

static TeaValue clear_list(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "clear() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }
    
    TeaObjectList* list = AS_LIST(args[0]);
    tea_init_value_array(&list->items);

    return EMPTY_VAL;
}

static TeaValue insert_list(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 2)
    {
        tea_runtime_error(vm, "insert() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[2]))
    {
        tea_runtime_error(vm, "insert() second argument must be a number");
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(args[0]);
    TeaValue insertValue = args[1];
    int index = AS_NUMBER(args[2]);

    if(index < 0 || index > list->items.count) 
    {
        tea_runtime_error(vm, "Index out of bounds for the list given");
        return EMPTY_VAL;
    }

    if(list->items.capacity < list->items.count + 1) 
    {
        int old_capacity = list->items.capacity;
        list->items.capacity = GROW_CAPACITY(old_capacity);
        list->items.values = GROW_ARRAY(vm->state, TeaValue, list->items.values, old_capacity, list->items.capacity);
    }

    list->items.count++;

    for(int i = list->items.count - 1; i > index; --i) 
    {
        list->items.values[i] = list->items.values[i - 1];
    }

    list->items.values[index] = insertValue;

    return EMPTY_VAL;
}

static TeaValue reverse_list(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "reverse() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(args[0]);
    int length = list->items.count;

    for(int i = 0; i < length / 2; i++) 
    {
        TeaValue temp = list->items.values[i];
        list->items.values[i] = list->items.values[length - i - 1];
        list->items.values[length - i - 1] = temp;
    }

    return OBJECT_VAL(list);
}

// Globals
static TeaValue print_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count == 0)
    {
        printf("\n");

        return EMPTY_VAL;
    }

    for(int i = 0; i < count; i++)
    {
        tea_print_value(args[i]);
        printf("\t");
    }

    printf("\n");

    return EMPTY_VAL;
}

static TeaValue input_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count < 0 || count > 1)
    {
        tea_runtime_error(vm, "input() expected either 0 or 1 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    if(count != 0) 
    {
        TeaValue prompt = args[0];
        if(!IS_STRING(prompt)) 
        {
            tea_runtime_error(vm, "input() only takes a string argument");
            return EMPTY_VAL;
        }

        printf("%s", AS_CSTRING(prompt));
    }

    uint64_t current_size = 128;
    char* line = ALLOCATE(vm->state, char, current_size);

    if(line == NULL) 
    {
        tea_runtime_error(vm, "Memory error on input()!");
        return EMPTY_VAL;
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

static TeaValue open_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 2)
    {
        tea_runtime_error(vm, "open() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]) || !IS_STRING(args[1]))
    {
        tea_runtime_error(vm, "open() expects two strings");
        return EMPTY_VAL;
    }

    TeaObjectFile* file = tea_new_file(vm->state);
    file->path = AS_STRING(args[0])->chars;
    file->type = AS_STRING(args[1])->chars;
    file->is_open = true;

    return OBJECT_VAL(file);
}

static TeaValue assert_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 2)
    {
        tea_runtime_error(vm, "assert() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(tea_is_falsey(args[0]))
    {
        tea_runtime_error(vm, "%s", AS_CSTRING(args[1]));
        return EMPTY_VAL;
    }

    return EMPTY_VAL;
}

static TeaValue error_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "error() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    tea_runtime_error(vm, "%s", AS_CSTRING(args[0]));

    return EMPTY_VAL;
}

static TeaValue type_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "type() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    const char* type = tea_value_type(args[0]);

    return OBJECT_VAL(tea_copy_string(vm->state, type, (int)strlen(type)));
}

static TeaValue number_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "number() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }
    
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
            tea_runtime_error(vm, "Failed conversion");
            return EMPTY_VAL;
        }

        return NUMBER_VAL(number);
    }
}

static TeaValue string_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "string() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    const char* string = tea_value_tostring(vm->state, args[0]);

    return OBJECT_VAL(tea_copy_string(vm->state, string, strlen(string)));
}

static TeaValue char_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "char() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectString* string = tea_ustring_from_code_point(vm->state, AS_NUMBER(args[0]));

    return OBJECT_VAL(string);
}

static TeaValue ord_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "ord() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(args[0]);
    int index = tea_ustring_decode((uint8_t*)string->chars, 1);

    return NUMBER_VAL(index);
}

static TeaValue gc_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "gc() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    tea_collect_garbage(vm);

    return EMPTY_VAL;
}

static TeaValue interpret_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "interpret() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "interpret() argument must be a string");
        return EMPTY_VAL;
    }

    TeaState* state = tea_init_state();
    tea_interpret(state, "interpret", AS_CSTRING(args[0]));
    tea_free_state(state);

    return EMPTY_VAL;
}

void tea_open_core(TeaVM* vm)
{
    // File
    tea_native_function(vm, &vm->file_methods, "write", write_file);
    tea_native_function(vm, &vm->file_methods, "writeline", writeline_file);
    tea_native_function(vm, &vm->file_methods, "read", read_file);
    tea_native_function(vm, &vm->file_methods, "readline", readline_file);
    tea_native_function(vm, &vm->file_methods, "seek", seek_file);

    // String
    tea_native_function(vm, &vm->string_methods, "reverse", reverse_string);

    // List
    tea_native_function(vm, &vm->list_methods, "add", add_list);
    tea_native_function(vm, &vm->list_methods, "remove", remove_list);
    tea_native_function(vm, &vm->list_methods, "clear", clear_list);
    tea_native_function(vm, &vm->list_methods, "insert", insert_list);
    tea_native_function(vm, &vm->list_methods, "reverse", reverse_list);

    // Globals
    tea_native_function(vm, &vm->globals, "print", print_native);
    tea_native_function(vm, &vm->globals, "input", input_native);
    tea_native_function(vm, &vm->globals, "open", open_native);
    tea_native_function(vm, &vm->globals, "assert", assert_native);
    tea_native_function(vm, &vm->globals, "error", error_native);
    tea_native_function(vm, &vm->globals, "type", type_native);
    tea_native_function(vm, &vm->globals, "gc", gc_native);
    tea_native_function(vm, &vm->globals, "interpret", interpret_native);
    tea_native_function(vm, &vm->globals, "number", number_native);
    //tea_native_function(vm, &vm->globals, "bool", bool_native);
    tea_native_function(vm, &vm->globals, "string", string_native);
    tea_native_function(vm, &vm->globals, "char", char_native);
    tea_native_function(vm, &vm->globals, "ord", ord_native);
    //tea_native_function(vm, &vm->globals, "list", list_native);
}