#include "types/tea_list.h"
#include "vm/tea_native.h"
/*
static TeaValue append_list(int arg_count, TeaValue* args)
{
    // Append a value to the end of a list increasing the list's length by 1
    if(arg_count != 1) 
    {
        // Handle error
    }

    TeaObjectList* list = AS_LIST(args[0]);
    TeaValue item = args[1];
    tea_append_to_list(list, item);

    return EMPTY_VAL;
}

static TeaValue delete_list(int arg_count, TeaValue* args)
{
    // Delete an item from a list at the given index.
    if(arg_count != 1 || !IS_NUMBER(args[1])) 
    {
        // Handle error
    }

    TeaObjectList* list = AS_LIST(args[0]);
    int index = AS_NUMBER(args[1]);

    if(!tea_is_valid_list_index(list, index)) 
    {
        // Handle error
    }

    tea_delete_from_list(list, index);

    return EMPTY_VAL;
}
*/

static TeaValue push_list(int arg_count, TeaValue* args)
{

}

static TeaValue pop_list(int arg_count, TeaValue* args)
{

}

void tea_define_list_methods(TeaVM* vm)
{
    //tea_native_function(vm, &vm->list_methods, "append", append_list);
    //tea_native_function(vm, &vm->list_methods, "delete", delete_list);
}