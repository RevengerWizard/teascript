/* 
** tea_core.c
** Teascript core function types
*/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

#include "tea_memory.h"
#include "tea_core.h"
#include "tea_utf.h"

// File
static TeaValue closed_file(TeaVM* vm, TeaValue instance)
{
    return BOOL_VAL(!AS_FILE(instance)->is_open);
}

static TeaValue path_file(TeaVM* vm, TeaValue instance)
{
    TeaObjectFile* file = AS_FILE(instance);
    return OBJECT_VAL(tea_take_string(vm->state, file->path, strlen(file->path)));
}

static TeaValue type_file(TeaVM* vm, TeaValue instance)
{
    TeaObjectFile* file = AS_FILE(instance);
    return OBJECT_VAL(tea_take_string(vm->state, file->type, strlen(file->type)));
}

static TeaValue write_file(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "write() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "write() argument must be a string");
        return EMPTY_VAL;
    }

    TeaObjectFile* file = AS_FILE(instance);
    TeaObjectString* string = AS_STRING(args[0]);

    if(strcmp(file->type, "r") == 0)
    {
        tea_runtime_error(vm, "File is not readable");
        return EMPTY_VAL;
    }

    int chars_wrote = fprintf(file->file, "%s", string->chars);
    fflush(file->file);

    return NUMBER_VAL(chars_wrote);
}

static TeaValue writeline_file(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "floor() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "writeline() argument must be a string");
        return EMPTY_VAL;
    }

    TeaObjectFile* file = AS_FILE(instance);
    TeaObjectString* string = AS_STRING(args[0]);

    if(strcmp(file->type, "r") == 0)
    {
        tea_runtime_error(vm, "File is not readable");
        return EMPTY_VAL;
    }

    int chars_wrote = fprintf(file->file, "%s\n", string->chars);
    fflush(file->file);

    return NUMBER_VAL(chars_wrote);
}

static TeaValue read_file(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "read() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }
    
    TeaObjectFile* file = AS_FILE(instance);

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

static TeaValue readline_file(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "readline() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    char line[4096];

    TeaObjectFile* file = AS_FILE(instance);
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

static TeaValue seek_file(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count < 1 || count > 2)
    {
        tea_runtime_error(vm, "seek() expected either 1 or 2 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    int seek_type = SEEK_SET;

    if(count == 2) 
    {
        if(!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) 
        {
            tea_runtime_error(vm, "seek() arguments must be numbers");
            return EMPTY_VAL;
        }

        int seek_type_num = AS_NUMBER(args[1]);

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

    if(!IS_NUMBER(args[0])) 
    {
        tea_runtime_error(vm, "seek() argument must be a number");
        return EMPTY_VAL;
    }

    int offset = AS_NUMBER(args[0]);
    TeaObjectFile* file = AS_FILE(instance);

    if(offset != 0 && !strstr(file->type, "b")) 
    {
        tea_runtime_error(vm, "seek() may not have non-zero offset if file is opened in text mode");
        return EMPTY_VAL;
    }

    fseek(file->file, offset, seek_type);

    return NULL_VAL;
}

static TeaValue close_file(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "close() takes 0 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectFile* file = AS_FILE(instance);
    if(!file->is_open)
    {
        tea_runtime_error(vm, "File is already closed");
        return EMPTY_VAL;
    }

    fclose(file->file);
    file->is_open = false;

    return EMPTY_VAL;
}

static TeaValue iterate_file(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    return readline_file(vm, instance, 0, args);
}

static TeaValue iteratorvalue_file(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    return args[0];
}

// List
static TeaValue len_list(TeaVM* vm, TeaValue instance)
{
    return NUMBER_VAL(AS_LIST(instance)->items.count);
}

static TeaValue add_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "add() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    if(IS_LIST(args[0]) && AS_LIST(args[0]) == list)
    {
        tea_runtime_error(vm, "Cannot add list into itself");
        return EMPTY_VAL;
    }
    
    tea_write_value_array(vm->state, &list->items, args[0]);

    return instance;
}

static TeaValue remove_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "remove() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    TeaValue remove = args[0];
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

static TeaValue delete_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0 && count != 1)
    {
        tea_runtime_error(vm, "delete() takes 0 or 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);

    if(list->items.count == 0) 
    {
        return EMPTY_VAL;
    }

    TeaValue element;

    if(count == 1)
    {
        if(!IS_NUMBER(args[0]))
        {
            tea_runtime_error(vm, "delete() argument must be a number");
            return EMPTY_VAL;
        }

        int index = AS_NUMBER(args[0]);

        if(index < 0 || index > list->items.count)
        {
            tea_runtime_error(vm, "Index out of bounds");
            return EMPTY_VAL;
        }

        element = list->items.values[index];

        for(int i = index; i < list->items.count - 1; ++i)
        {
            list->items.values[i] = list->items.values[i + 1];
        }
    }
    else
    {
        element = list->items.values[list->items.count - 1];
    }

    list->items.count--;

    return element;
}

static TeaValue clear_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "clear() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }
    
    TeaObjectList* list = AS_LIST(instance);
    tea_init_value_array(&list->items);

    return EMPTY_VAL;
}

static TeaValue insert_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
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

    TeaObjectList* list = AS_LIST(instance);
    TeaValue insert_value = args[0];
    int index = AS_NUMBER(args[1]);

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

    list->items.values[index] = insert_value;

    return EMPTY_VAL;
}

static TeaValue extend_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "extend() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_LIST(args[0]))
    {
        tea_runtime_error(vm, "extend() argument must be a list");
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    TeaObjectList* argument = AS_LIST(args[0]);

    for(int i = 0; i < argument->items.count; i++) 
    {
        tea_write_value_array(vm->state, &list->items, argument->items.values[i]);
    }

    return EMPTY_VAL;
}

static TeaValue contains_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "insert() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    TeaValue search = args[0];

    for(int i = 0; i < list->items.count; i++) 
    {
        if(tea_values_equal(list->items.values[i], search)) 
        {
            return TRUE_VAL;
        }
    }

    return FALSE_VAL;
}

static TeaValue count_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "count() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    TeaValue value = args[0];
    int n = 0;

    for(int i = 0; i < list->items.count; i++) 
    {
        if(tea_values_equal(list->items.values[i], value)) 
        {
            n++;
        }
    }

    return NUMBER_VAL(n);
}

static TeaValue swap_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 2)
    {
        tea_runtime_error(vm, "swap() takes 2 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0]) || !IS_NUMBER(args[1]))
    {
        tea_runtime_error(vm, "swap() takes two numbers as arguments");
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    int index_a = AS_NUMBER(args[0]);
    int index_b = AS_NUMBER(args[1]);
    if(index_a < 0 || index_a > list->items.count || index_b < 0 || index_b > list->items.count)
    {
        tea_runtime_error(vm, "Index out of bounds");
        return EMPTY_VAL;
    }

    TeaValue value = list->items.values[index_a];
    list->items.values[index_a] = list->items.values[index_b];
    list->items.values[index_b] = value;

    return EMPTY_VAL;
}

static TeaValue fill_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "fill() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    TeaValue value = args[0];

    for(int i = 0; i < list->items.count; i++) 
    {
        list->items.values[i] = value;
    }

    return EMPTY_VAL;
}

static TeaValue reverse_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "reverse() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    int length = list->items.count;

    for(int i = 0; i < length / 2; i++) 
    {
        TeaValue temp = list->items.values[i];
        list->items.values[i] = list->items.values[length - i - 1];
        list->items.values[length - i - 1] = temp;
    }

    return OBJECT_VAL(list);
}

static int partition(TeaObjectList* arr, int start, int end) 
{
    int pivot_index = (int)floor(start + end) / 2;

    double pivot =  AS_NUMBER(arr->items.values[pivot_index]);

    int i = start - 1;
    int j = end + 1;

    while(true)
    {
        do 
        {
            i = i + 1;
        } 
        while(AS_NUMBER(arr->items.values[i]) < pivot);

        do 
        {
            j = j - 1;
        } 
        while(AS_NUMBER(arr->items.values[j]) > pivot);

        if (i >= j) 
        {
            return j;
        }

        // Swap arr[i] with arr[j]
        TeaValue temp = arr->items.values[i];
        
        arr->items.values[i] = arr->items.values[j];
        arr->items.values[j] = temp;
    }
}

// Implementation of Quick Sort using the Hoare
// Partition scheme
// Best Case O(n log n)
// Worst Case O(n^2) (If the list is already sorted) 
static void quicksort(TeaObjectList* arr, int start, int end) 
{
    while(start < end) 
    {
        int part = partition(arr, start, end);

        // Recurse for the smaller halve.
        if(part - start < end - part) 
        {
            quicksort(arr, start, part);
            
            start = start + 1;
        } 
        else 
        {
            quicksort(arr, part + 1, end);

            end = end - 1;
        }
    }
}

static TeaValue sort_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "sort() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);

    // Check if all the list elements are indeed numbers.
    for(int i = 0; i < list->items.count; i++) 
    {
        if(!IS_NUMBER(list->items.values[i])) {
            tea_runtime_error(vm, "sort() takes lists with numbers (index %d was not a number)", i);
            return EMPTY_VAL;
        }
    }

    quicksort(list, 0, list->items.count - 1);

    return EMPTY_VAL;
}

static TeaValue index_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "index() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    TeaValue value = args[0];

    count = list->items.count;
    for(int i = 0; i < count; i++)
    {
        TeaValue item = list->items.values[i];
        if(tea_values_equal(item, value)) 
        {
            return NUMBER_VAL(i);
        }
    }
    return NULL_VAL;
}

static TeaValue join_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count < 0 || count > 1)
    {
        tea_runtime_error(vm, "join() expected either 0 or 1 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    TeaObjectList* list = AS_LIST(instance);
    if(list->items.count == 0) return OBJECT_VAL(tea_copy_string(vm->state, "", 0));

    char* delimiter = "";
    if(count == 1)
    {
        if(!IS_STRING(args[0]))
        {
            tea_runtime_error(vm, "join() takes a string as argument", count);
            return EMPTY_VAL;
        }

        delimiter = AS_CSTRING(args[0]);
    }

    char* output;
    char* string = NULL;
    int length = 0;
    int delimiterLength = strlen(delimiter);

    for(int i = 0; i < list->items.count - 1; i++)
    {
        if(IS_STRING(list->items.values[i]))
        {
            output = AS_CSTRING(list->items.values[i]);
        } 
        else 
        {
            output = tea_value_tostring(vm->state, list->items.values[i]);
        }
        int elementLength = strlen(output);

        string = GROW_ARRAY(vm->state, char, string, length, length + elementLength + delimiterLength);

        memcpy(string + length, output, elementLength);
        if(!IS_STRING(list->items.values[i])) 
        {
            FREE(vm->state, char, output);
        }
        length += elementLength;
        memcpy(string + length, delimiter, delimiterLength);
        length += delimiterLength;
    }

    // Outside the loop as we do not want the append the delimiter on the last element
    if(IS_STRING(list->items.values[list->items.count - 1])) 
    {
        output = AS_CSTRING(list->items.values[list->items.count - 1]);
    } 
    else 
    {
        output = tea_value_tostring(vm->state, list->items.values[list->items.count - 1]);
    }

    int elementLength = strlen(output);
    string = GROW_ARRAY(vm->state, char, string, length, length + elementLength + 1);
    memcpy(string + length, output, elementLength);
    length += elementLength;

    string[length] = '\0';

    if(!IS_STRING(list->items.values[list->items.count - 1]))
    {
        FREE(vm->state, char, output);
    }

    return OBJECT_VAL(tea_take_string(vm->state, string, length));
}

static TeaValue iterate_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaObjectList* list = AS_LIST(instance);

    // If we're starting the iteration, return the first index.
    if(IS_NULL(args[0]))
    {
        if(list->items.count == 0) return NULL_VAL;
        return NUMBER_VAL(0);
    }

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "Expected a number to iterate");
        return EMPTY_VAL;
    }

    int index = AS_NUMBER(args[0]);
    // Stop if we're out of bounds.
    if(index < 0 || index >= list->items.count - 1) return NULL_VAL;

    // Otherwise, move to the next index.
    return NUMBER_VAL(index + 1);
}

static TeaValue iteratorvalue_list(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaObjectList* list = AS_LIST(instance);
    int index = AS_NUMBER(args[0]);

    return list->items.values[index];
}

// Map
static TeaValue len_map(TeaVM* vm, TeaValue instance)
{
    return NUMBER_VAL(AS_MAP(instance)->count);
}

static TeaValue keys_map(TeaVM* vm, TeaValue instance)
{
    TeaObjectMap* map = AS_MAP(instance);

    TeaObjectList* list = tea_new_list(vm->state);

    for(int i = 0; i < map->capacity + 1; ++i)
    {
        if(map->items[i].empty)
        {
            continue;
        }

        tea_write_value_array(vm->state, &list->items, map->items[i].key);
    }

    return OBJECT_VAL(list);
}

static TeaValue values_map(TeaVM* vm, TeaValue instance)
{
    TeaObjectMap* map = AS_MAP(instance);

    TeaObjectList* list = tea_new_list(vm->state);

    for(int i = 0; i < map->capacity + 1; ++i)
    {
        if(map->items[i].empty)
        {
            continue;
        }

        tea_write_value_array(vm->state, &list->items, map->items[i].value);
    }

    return OBJECT_VAL(list);
}

static TeaValue clear_map(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "clear() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectMap* map = AS_MAP(instance);
    map->items = NULL;
    map->capacity = 0;
    map->count = 0;

    return EMPTY_VAL;
}

static TeaValue contains_map(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "contains() takes 1 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectMap* map = AS_MAP(instance);

    if(!tea_is_valid_key(args[0]))
    {
        tea_runtime_error(vm, "Map key isn't hashable");
        return EMPTY_VAL;
    }
    
    TeaValue _;
    return BOOL_VAL(tea_map_get(map, args[0], &_));
}

static TeaValue remove_map(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "remove() takes 1 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectMap* map = AS_MAP(instance);
    TeaValue _;
    if(!tea_is_valid_key(args[0]))
    {
        tea_runtime_error(vm, "Map key isn't hashable");
        return EMPTY_VAL;
    }
    else if(!tea_map_get(map, args[0], &_))
    {
        tea_runtime_error(vm, "No such key in the map");
        return EMPTY_VAL;
    }

    tea_map_delete(map, args[0]);

    return args[0];
}

static TeaValue iterate_map(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaObjectMap* map = AS_MAP(instance);

    if(map->count == 0) return NULL_VAL;

    // If we're starting the iteration, start at the first used entry.
    int index = 0;

    // Otherwise, start one past the last entry we stopped at.
    if(!IS_NULL(args[0]))
    {
        if(!IS_NUMBER(args[0]))
        {
            tea_runtime_error(vm, "Expected a number to iterate");
            return EMPTY_VAL;
        }

        if(AS_NUMBER(args[0]) < 0) return NULL_VAL;
        index = (uint32_t)AS_NUMBER(args[0]);

        if(index >= map->capacity) return NULL_VAL;

        // Advance the iterator.
        index++;
    }

    // Find a used entry, if any.
    for(; index < map->capacity; index++)
    {
        if (!map->items[index].empty) return NUMBER_VAL(index);
    }

    // If we get here, walked all of the entries.
    return NULL_VAL;
}

static TeaValue iteratorvalue_map(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaObjectMap* map = AS_MAP(instance);
    int index = AS_NUMBER(args[0]);

    TeaMapItem* item = &map->items[index];
    if(item->empty)
    {
        tea_runtime_error(vm, "Invalid map iterator");
        return EMPTY_VAL;
    }

    TeaObjectMap* value = tea_new_map(vm->state);
    tea_push(vm, OBJECT_VAL(value));
    tea_map_set(vm->state, value, OBJECT_VAL(tea_copy_string(vm->state, "key", 3)), item->key);
    tea_map_set(vm->state, value, OBJECT_VAL(tea_copy_string(vm->state, "value", 5)), item->value);
    tea_pop(vm);

    return OBJECT_VAL(value);
}

// String
static TeaValue len_string(TeaVM* vm, TeaValue instance)
{
    return NUMBER_VAL(tea_ustring_length(AS_STRING(instance)));
}

static TeaValue upper_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "upper() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(instance);
    char* temp = ALLOCATE(vm->state, char, string->length + 1);

    for(int i = 0; string->chars[i]; i++) 
    {
        temp[i] = toupper(string->chars[i]);
    }
    temp[string->length] = '\0';

    return OBJECT_VAL(tea_take_string(vm->state, temp, string->length));
}

static TeaValue lower_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "lower() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(instance);
    char* temp = ALLOCATE(vm->state, char, string->length + 1);

    for(int i = 0; string->chars[i]; i++) 
    {
        temp[i] = tolower(string->chars[i]);
    }
    temp[string->length] = '\0';

    return OBJECT_VAL(tea_take_string(vm->state, temp, string->length));
}

static TeaValue reverse_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "reverse() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    const char* string = AS_CSTRING(instance);

    int l = strlen(string);

    if(l == 0 || l == 1)
    {
        return OBJECT_VAL(AS_STRING(instance));
    }

    char* res = ALLOCATE(vm->state, char, l + 1);
    for(int i = 0; i < l; i++)
    {
        res[i] = string[l - i - 1];
    }
    res[l] = '\0';

    return OBJECT_VAL(tea_take_string(vm->state, res, l));
}

static TeaValue split_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count < 0 || count > 2)
    {
        tea_runtime_error(vm, "split() expected either 0 to 2 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(instance);
    char* delimiter;
    int max_split = string->length + 1;

    if(count == 0)
    {
        delimiter = " ";
    }
    else if(count > 0)
    {
        if(!IS_STRING(args[0]))
        {
            tea_runtime_error(vm, "split() argument must be a string");
            return EMPTY_VAL;
        }

        delimiter = AS_CSTRING(args[0]);

        if(count == 2)
        {
            if(!IS_NUMBER(args[1]))
            {
                tea_runtime_error(vm, "split() argument must be a number");
                return EMPTY_VAL;
            }

            if(AS_NUMBER(args[1]) >= 0)
            {
                max_split = AS_NUMBER(args[1]);
            }
        }
    }

    char* temp = ALLOCATE(vm->state, char, string->length + 1);
    char* temp_free = temp;
    memcpy(temp, string->chars, string->length);
    temp[string->length] = '\0';
    int delimeter_length = strlen(delimiter);
    char *token;

    TeaObjectList* list = tea_new_list(vm->state);
    count = 0;

    if(delimeter_length == 0) 
    {
        int tokenIndex = 0;
        for(; tokenIndex < string->length && count < max_split; tokenIndex++) 
        {
            count++;
            *(temp) = string->chars[tokenIndex];
            *(temp + 1) = '\0';
            TeaValue str = OBJECT_VAL(tea_copy_string(vm->state, temp, 1));
            tea_write_value_array(vm->state, &list->items, str);
        }

        if(tokenIndex != string->length && count >= max_split)
        {
            temp = (string->chars) + tokenIndex;
        } 
        else 
        {
            temp = NULL;
        }
    } 
    else if(max_split > 0) 
    {
        do 
        {
            count++;
            token = strstr(temp, delimiter);
            if(token)
            {
                *token = '\0';
            }

            TeaValue str = OBJECT_VAL(tea_copy_string(vm->state, temp, strlen(temp)));
            tea_write_value_array(vm->state, &list->items, str);
            temp = token + delimeter_length;
        } 
        while (token != NULL && count < max_split);

        if(token == NULL) 
        {
            temp = NULL;
        }
    }

    if(temp != NULL && count >= max_split)
    {
        TeaValue rest = OBJECT_VAL(tea_copy_string(vm->state, temp, strlen(temp)));
        tea_write_value_array(vm->state, &list->items, rest);
    }

    FREE_ARRAY(vm->state, char, temp_free, string->length + 1);
    return OBJECT_VAL(list);
}

static TeaValue title_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "title() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(instance);
    char* temp = ALLOCATE(vm->state, char, string->length + 1);

    bool convertNext = true;

    for(int i = 0; string->chars[i]; i++) 
    {
        if(string->chars[i]==' ')
        {
            convertNext=true;
        }
        else if(convertNext)
        {
            temp[i] = toupper(string->chars[i]);
            convertNext=false;
            continue;
        }
        temp[i] = tolower(string->chars[i]);
    }

    temp[string->length] = '\0';

    return OBJECT_VAL(tea_take_string(vm->state, temp, string->length));
}

static TeaValue contains_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "contains() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "contains() argument must be a string");
        return EMPTY_VAL;
    }

    char* string = AS_CSTRING(instance);
    char* delimiter = AS_CSTRING(args[0]);

    return !strstr(string, delimiter) ? FALSE_VAL : TRUE_VAL;
}

static TeaValue startswith_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "startswith() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "startswith() argument must be a string");
        return EMPTY_VAL;
    }

    char* string = AS_CSTRING(instance);
    TeaObjectString* start = AS_STRING(args[0]);

    return BOOL_VAL(strncmp(string, start->chars, start->length) == 0);
}

static TeaValue endswith_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "endswith() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "endswith() argument must be a string");
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(instance);
    TeaObjectString* end = AS_STRING(args[0]);

    return BOOL_VAL(strcmp(string->chars + (string->length - end->length), end->chars) == 0);
}

static TeaValue leftstrip_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "leftstrip() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(instance);
    int i = 0;
    count = 0;
    char* temp = ALLOCATE(vm->state, char, string->length + 1);

    for(i = 0; i < string->length; i++) 
    {
        if(!isspace(string->chars[i]))
        {
            break;
        }
        count++;
    }

    if(count != 0) 
    {
        temp = GROW_ARRAY(vm->state, char, temp, string->length + 1, (string->length - count) + 1);
    }

    memcpy(temp, string->chars + count, string->length - count);
    temp[string->length - count] = '\0';

    return OBJECT_VAL(tea_take_string(vm->state, temp, string->length - count));
}

static TeaValue rightstrip_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "rightstrip() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(instance);
    int length;
    char* temp = ALLOCATE(vm->state, char, string->length + 1);

    for(length = string->length - 1; length > 0; length--) 
    {
        if(!isspace(string->chars[length]))
        {
            break;
        }
    }

    // If characters were stripped resize the buffer
    if(length + 1 != string->length) 
    {
        temp = GROW_ARRAY(vm->state, char, temp, string->length + 1, length + 2);
    }

    memcpy(temp, string->chars, length + 1);
    temp[length + 1] = '\0';

    return OBJECT_VAL(tea_take_string(vm->state, temp, length + 1));
}

static TeaValue strip_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 0)
    {
        tea_runtime_error(vm, "strip() takes 0 arguments (%d given)", count);
        return EMPTY_VAL;
    }

    TeaValue string = leftstrip_string(vm, instance, 0, args);
    string = rightstrip_string(vm, string, 0, &string);

    return string;
}

static TeaValue count_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "count() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "count() argument must be a string");
        return EMPTY_VAL;
    }

    char* string = AS_CSTRING(instance);
    char* needle = AS_CSTRING(args[0]);

    count = 0;
    while((string = strstr(string, needle))) 
    {
        count++;
        string++;
    }

    return NUMBER_VAL(count);
}

static TeaValue find_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count < 1 || count > 2)
    {
        tea_runtime_error(vm, "find() expected either 1 or 2 arguments (got %d)", count);
        return EMPTY_VAL;
    }

    int index = 1;

    if(count == 2)
    {
        if(!IS_NUMBER(args[1]))
        {
            tea_runtime_error(vm, "count() argument must be a string");
            return EMPTY_VAL;
        }

        index = AS_NUMBER(args[1]);
    }

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "count() substring must be a string");
        return EMPTY_VAL;
    }

    char* substr = AS_CSTRING(args[0]);
    char* string = AS_CSTRING(instance);

    int position = 0;

    for(int i = 0; i < index; i++) 
    {
        char *result = strstr(string, substr);
        if(!result) 
        {
            position = -1;
            break;
        }

        position += (result - string) + (i * strlen(substr));
        string = result + strlen(substr);
    }

    return NUMBER_VAL(position);
}

static TeaValue format_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    if(count == 0)
    {
        tea_runtime_error(vm, "format() takes at least 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    int length = 0;
    char** replaceStrings = ALLOCATE(vm->state, char*, count);

    for(int j = 0; j < count + 1; j++) 
    {
        TeaValue value = args[j];
        if(!IS_STRING(value))
        {
            replaceStrings[j] = tea_value_tostring(vm->state, value);
        }
        else 
        {
            TeaObjectString* strObj = AS_STRING(value);
            char* str = malloc(strObj->length + 1);
            memcpy(str, strObj->chars, strObj->length + 1);
            replaceStrings[j] = str;
        }

        length += strlen(replaceStrings[j]);
    }

    TeaObjectString* string = AS_STRING(instance);

    int stringLen = string->length + 1;
    char* tmp = ALLOCATE(vm->state, char, stringLen);
    char* tmpFree = tmp;
    memcpy(tmp, string->chars, stringLen);

    int n = 0;
    while((tmp = strstr(tmp, "{}")))
    {
        n++;
        tmp++;
    }

    tmp = tmpFree;

    if(n != count) 
    {
        tea_runtime_error(vm, "format() placeholders do not match arguments");

        for(int i = 0; i < count; i++)
        {
            free(replaceStrings[i]);
        }

        FREE_ARRAY(vm->state, char, tmp , stringLen);
        FREE_ARRAY(vm->state, char*, replaceStrings, count);
        return EMPTY_VAL;
    }

    int fullLength = string->length - n * 2 + length + 1;
    char* pos;
    char* newStr = ALLOCATE(vm->state, char, fullLength);
    int stringLength = 0;

    for(int i = 0; i < count; i++) 
    {
        pos = strstr(tmp, "{}");
        if(pos != NULL)
            *pos = '\0';

        int tmpLength = strlen(tmp);
        int replaceLength = strlen(replaceStrings[i]);
        memcpy(newStr + stringLength, tmp, tmpLength);
        memcpy(newStr + stringLength + tmpLength, replaceStrings[i], replaceLength);
        stringLength += tmpLength + replaceLength;
        tmp = pos + 2;
        free(replaceStrings[i]);
    }

    FREE_ARRAY(vm->state, char*, replaceStrings, count);
    memcpy(newStr + stringLength, tmp, strlen(tmp));
    newStr[fullLength - 1] = '\0';
    FREE_ARRAY(vm->state, char, tmpFree, stringLen);

    return OBJECT_VAL(tea_take_string(vm->state, newStr, fullLength - 1));
}

static TeaValue iterate_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaObjectString* string = AS_STRING(instance);

	if(IS_NULL(args[0]))
    {
		if(string->length == 0) return NULL_VAL;
		return NUMBER_VAL(0);
	}

	if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "Expected a number to iterate");
        return EMPTY_VAL;
    }
	if(AS_NUMBER(args[0]) < 0) return NULL_VAL;

    int index = AS_NUMBER(args[0]);
	do
    {
		index++;
		if(index >= string->length) return NULL_VAL;
	}
    while((string->chars[index] & 0xc0) == 0x80);

	return NUMBER_VAL(index);
}

static TeaValue iteratorvalue_string(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaObjectString* string = AS_STRING(instance);
	int index = AS_NUMBER(args[0]);

	return OBJECT_VAL(tea_ustring_code_point_at(vm->state, string, index));
}

// Range
static TeaValue iterate_range(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    TeaObjectRange* range = AS_RANGE(instance);

    // Special case: empty range.
    if(range->from == range->to && !range->inclusive) return NULL_VAL;

    // Start the iteration.
    if(IS_NULL(args[0])) return NUMBER_VAL(range->from);

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "Expected a number to iterate");
        return EMPTY_VAL;
    }

    int iterator = AS_NUMBER(args[0]);

    // Iterate towards [to] from [from].
    if(range->from < range->to)
    {
        iterator++;
        if(iterator > range->to) return NULL_VAL;
    }
    else
    {
        iterator--;
        if(iterator < range->to) return NULL_VAL;
    }

    if(!range->inclusive && iterator == range->to) return NULL_VAL;

    return NUMBER_VAL(iterator);
}

static TeaValue iteratorvalue_range(TeaVM* vm, TeaValue instance, int count, TeaValue* args)
{
    return args[0];
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

    char* path = AS_CSTRING(args[0]);
    char* type = AS_CSTRING(args[1]);

    TeaObjectFile* file = tea_new_file(vm->state);
    file->file = fopen(path, type);
    file->path = path;
    file->type = type;
    file->is_open = true;

    if (file->file == NULL) 
    {
        tea_runtime_error(vm, "Unable to open file '%s'", file->path);
        return EMPTY_VAL;
    }

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

    return args[0];
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

    char* string = tea_value_tostring(vm->state, args[0]);

    return OBJECT_VAL(tea_take_string(vm->state, string, strlen(string)));
}

static TeaValue char_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 1)
    {
        tea_runtime_error(vm, "char() takes 1 argument (%d given)", count);
        return EMPTY_VAL;
    }

    if(!IS_NUMBER(args[0]))
    {
        tea_runtime_error(vm, "char() argument must be a number");
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

    if(!IS_STRING(args[0]))
    {
        tea_runtime_error(vm, "ord() argument must be a string");
        return EMPTY_VAL;
    }

    TeaObjectString* string = AS_STRING(args[0]);
    int index = tea_ustring_decode((uint8_t*)string->chars, 1);

    return NUMBER_VAL(index);
}

static TeaValue gc_native(TeaVM* vm, int count, TeaValue* args)
{
    if(count != 0)
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
    tea_native_property(vm, &vm->file_methods, "closed", closed_file);
    tea_native_property(vm, &vm->file_methods, "path", path_file);
    tea_native_property(vm, &vm->file_methods, "type", type_file);
    tea_native_method(vm, &vm->file_methods, "write", write_file);
    tea_native_method(vm, &vm->file_methods, "writeline", writeline_file);
    tea_native_method(vm, &vm->file_methods, "read", read_file);
    tea_native_method(vm, &vm->file_methods, "readline", readline_file);
    tea_native_method(vm, &vm->file_methods, "seek", seek_file);
    tea_native_method(vm, &vm->file_methods, "close", close_file);
    tea_native_method(vm, &vm->file_methods, "iterate", iterate_file);
    tea_native_method(vm, &vm->file_methods, "iteratorvalue", iteratorvalue_file);

    // List
    tea_native_property(vm, &vm->list_methods, "len", len_list);
    tea_native_method(vm, &vm->list_methods, "add", add_list);
    tea_native_method(vm, &vm->list_methods, "remove", remove_list);
    tea_native_method(vm, &vm->list_methods, "delete", delete_list);
    tea_native_method(vm, &vm->list_methods, "clear", clear_list);
    tea_native_method(vm, &vm->list_methods, "insert", insert_list);
    tea_native_method(vm, &vm->list_methods, "extend", extend_list);
    tea_native_method(vm, &vm->list_methods, "reverse", reverse_list);
    tea_native_method(vm, &vm->list_methods, "contains", contains_list);
    tea_native_method(vm, &vm->list_methods, "count", count_list);
    tea_native_method(vm, &vm->list_methods, "swap", swap_list);
    tea_native_method(vm, &vm->list_methods, "fill", fill_list);
    tea_native_method(vm, &vm->list_methods, "sort", sort_list);
    tea_native_method(vm, &vm->list_methods, "index", index_list);
    tea_native_method(vm, &vm->list_methods, "join", join_list);
    //tea_native_method(vm, &vm->list_methods, "copy", copy_list);
    tea_native_method(vm, &vm->list_methods, "iterate", iterate_list);
    tea_native_method(vm, &vm->list_methods, "iteratorvalue", iteratorvalue_list);

    // Map
    tea_native_property(vm, &vm->map_methods, "len", len_map);
    tea_native_property(vm, &vm->map_methods, "keys", keys_map);
    tea_native_property(vm, &vm->map_methods, "values", values_map);
    tea_native_method(vm, &vm->map_methods, "clear", clear_map);
    tea_native_method(vm, &vm->map_methods, "contains", contains_map);
    tea_native_method(vm, &vm->map_methods, "remove", remove_map);
    tea_native_method(vm, &vm->map_methods, "iterate", iterate_map);
    tea_native_method(vm, &vm->map_methods, "iteratorvalue", iteratorvalue_map);

    // String
    tea_native_property(vm, &vm->string_methods, "len", len_string);
    tea_native_method(vm, &vm->string_methods, "upper", upper_string);
    tea_native_method(vm, &vm->string_methods, "lower", lower_string);
    tea_native_method(vm, &vm->string_methods, "reverse", reverse_string);
    tea_native_method(vm, &vm->string_methods, "split", split_string);
    tea_native_method(vm, &vm->string_methods, "title", title_string);
    tea_native_method(vm, &vm->string_methods, "contains", contains_string);
    tea_native_method(vm, &vm->string_methods, "startswith", startswith_string);
    tea_native_method(vm, &vm->string_methods, "endswith", endswith_string);
    tea_native_method(vm, &vm->string_methods, "leftstrip", leftstrip_string);
    tea_native_method(vm, &vm->string_methods, "rightstrip", rightstrip_string);
    tea_native_method(vm, &vm->string_methods, "strip", strip_string);
    tea_native_method(vm, &vm->string_methods, "count", count_string);
    tea_native_method(vm, &vm->string_methods, "find", find_string);
    tea_native_method(vm, &vm->string_methods, "format", format_string);
    tea_native_method(vm, &vm->string_methods, "iterate", iterate_string);
    tea_native_method(vm, &vm->string_methods, "iteratorvalue", iteratorvalue_string);

    // Range
    tea_native_method(vm, &vm->range_methods, "iterate", iterate_range);
    tea_native_method(vm, &vm->range_methods, "iteratorvalue", iteratorvalue_range);

    // Globals
    tea_native_function(vm, &vm->globals, "print", print_native);
    tea_native_function(vm, &vm->globals, "input", input_native);
    tea_native_function(vm, &vm->globals, "open", open_native);
    tea_native_function(vm, &vm->globals, "assert", assert_native);
    tea_native_function(vm, &vm->globals, "error", error_native);
    tea_native_function(vm, &vm->globals, "type", type_native);
    tea_native_function(vm, &vm->globals, "gc", gc_native);
    tea_native_function(vm, &vm->globals, "interpret", interpret_native);
    tea_native_function(vm, &vm->globals, "char", char_native);
    tea_native_function(vm, &vm->globals, "ord", ord_native);
    tea_native_function(vm, &vm->globals, "number", number_native);
    //tea_native_function(vm, &vm->globals, "bool", bool_native);
    tea_native_function(vm, &vm->globals, "string", string_native);
    //tea_native_function(vm, &vm->globals, "list", list_native);
}