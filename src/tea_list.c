#include "tea_type.h"
#include "tea_native.h"

static TeaValue add_list(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(add, 1);

    TeaObjectList* list = AS_LIST(args[0]);
    if(IS_LIST(args[1]) && AS_LIST(args[1]) == list)
    {
        NATIVE_ERROR("Cannot add list into itself.");
    }
    
    tea_write_value_array(vm->state, &list->items, args[1]);

    return EMPTY_VAL;
}

static TeaValue remove_list(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(remove, 1);

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

    NATIVE_ERROR("Value does not exist within the list");
}

static TeaValue clear_list(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(clear, 0);

    TeaObjectList* list = AS_LIST(args[0]);
    tea_init_value_array(&list->items);

    return EMPTY_VAL;
}

static TeaValue insert_list(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(insert, 2);

    if(!IS_NUMBER(args[2]))
    {
        NATIVE_ERROR("insert() second argument must be a number.");
    }

    TeaObjectList* list = AS_LIST(args[0]);
    TeaValue insertValue = args[1];
    int index = AS_NUMBER(args[2]);

    if(index < 0 || index > list->items.count) 
    {
        NATIVE_ERROR("Index out of bounds for the list given");
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

static TeaValue reverse_list(TeaVM* vm, int arg_count, TeaValue* args, bool* error)
{
    VALIDATE_ARG_COUNT(reverse, 0);

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

void tea_define_list_methods(TeaVM* vm)
{
    tea_native_function(vm, &vm->list_methods, "add", add_list);
    tea_native_function(vm, &vm->list_methods, "remove", remove_list);
    tea_native_function(vm, &vm->list_methods, "clear", clear_list);
    tea_native_function(vm, &vm->list_methods, "insert", insert_list);
    tea_native_function(vm, &vm->list_methods, "reverse", reverse_list);
}