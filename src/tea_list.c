// tea_list.c
// Teascript list class

#include <math.h>

#include "tea.h"

#include "tea_vm.h"
#include "tea_memory.h"
#include "tea_core.h"

static void list_len(TeaState* T)
{
    tea_push_number(T, tea_len(T, 0));
}

static void list_add(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);
    tea_add_item(T, 0);
}

static void list_remove(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    int len = tea_len(T, 0);

    bool found = false;
    if(len == 0) 
    {
        tea_pop(T, 1);
        return;
    }

    if(len > 1) 
    {
        for(int i = 0; i < len - 1; i++) 
        {
            tea_get_item(T, 0, i);
            if(!found && tea_equals(T, 1, 2)) 
            {
                found = true;
            }
            tea_pop(T, 1);

            // If we have found the value, shuffle the array
            if(found) 
            {
                tea_get_item(T, 0, i + 1);
                tea_set_item(T, 0, i);
            }
        }

        // Check if it's the last element
        tea_get_item(T, 0, len - 1);
        if(!found && tea_equals(T, 1, 2)) 
        {
            found = true;
        }
        tea_pop(T, 1);
    } 
    else 
    {
        tea_get_item(T, 0, 0);
        if(tea_equals(T, 1, 2)) 
        {
            found = true;
        }
        tea_pop(T, 1);
    }

    if(found) 
    {
        AS_LIST(T->slot[0])->items.count--;
        tea_pop(T, 1);
        return;
    }

    tea_error(T, "Value does not exist within the list");
}

static void list_delete(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    int len = tea_len(T, 0);

    if(len == 0) 
    {
        tea_pop(T, 1);
        return;
    }

    int index = tea_check_int(T, 1);

    if(index < 0 || index > len)
    {
        tea_error(T, "Index out of bounds");
    }

    for(int i = index; i < len - 1; i++)
    {
        tea_get_item(T, 0, i + 1);
        tea_set_item(T, 0, i);
    }

    AS_LIST(T->slot[0])->items.count--;
    tea_pop(T, 1);
}

static void list_clear(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);
    
    TeaObjectList* list = AS_LIST(T->slot[0]);
    tea_init_value_array(&list->items);
}

static void list_insert(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 3);

    TeaObjectList* list = AS_LIST(T->slot[0]);
    TeaValue insert_value = T->slot[1];
    int index = tea_check_number(T, 2);

    if(index < 0 || index > list->items.count) 
    {
        tea_error(T, "Index out of bounds for the list given");
    }

    if(list->items.capacity < list->items.count + 1) 
    {
        int old_capacity = list->items.capacity;
        list->items.capacity = GROW_CAPACITY(old_capacity);
        list->items.values = GROW_ARRAY(T, TeaValue, list->items.values, old_capacity, list->items.capacity);
    }

    list->items.count++;

    for(int i = list->items.count - 1; i > index; --i) 
    {
        list->items.values[i] = list->items.values[i - 1];
    }

    list->items.values[index] = insert_value;
    tea_pop(T, 2);
}

static void list_extend(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    tea_check_list(T, 1);
    int len = tea_len(T, 1);

    // list index 0, list index 1
    for(int i = 0; i < len; i++) 
    {
        tea_get_item(T, 1, i);
        tea_add_item(T, 0);
    }
    tea_pop(T, 1);
}

static void list_reverse(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    int len = tea_len(T, 0);
    for(int i = 0; i < len / 2; i++) 
    {
        tea_get_item(T, 0, i);
        tea_get_item(T, 0, len - i - 1);
        tea_set_item(T, 0, i);
        tea_set_item(T, 0, len - i - 1);
    }
}

static void list_contains(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    int len = tea_len(T, 0);

    for(int i = 0; i < len; i++) 
    {
        tea_get_item(T, 0, i);
        if(tea_equals(T, 1, 2)) 
        {
            tea_push_bool(T, true);
            return;
        }
        tea_pop(T, 1);
    }

    tea_push_bool(T, false);
}

static void list_count(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    int len = tea_len(T, 0);

    int n = 0;
    for(int i = 0; i < len; i++) 
    {
        tea_get_item(T, 0, i);
        if(tea_equals(T, 1, 2)) 
        {
            n++;
        }
        tea_pop(T, 1);
    }

    tea_push_number(T, n);
}

static void list_swap(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 3);

    int len = tea_len(T, 0);

    int index_a = tea_check_number(T, 1);
    int index_b = tea_check_number(T, 2);

    if(index_a < 0 || index_a > len || index_b < 0 || index_b > len)
    {
        tea_error(T, "Index out of bounds");
    }

    tea_pop(T, 2);
    tea_get_item(T, 0, index_a);
    tea_get_item(T, 0, index_b);
    tea_set_item(T, 0, index_a);
    tea_set_item(T, 0, index_b);
}

static void list_fill(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    int len = tea_len(T, 0);

    int n = 0;
    for(int i = 0; i < len; i++) 
    {
        tea_push_value(T, 1);
        tea_set_item(T, 0, i);
    }
    tea_pop(T, 1);
}

static int partition(TeaState* T, int start, int end) 
{
    int pivot_index = (int)floor(start + end) / 2;

    tea_get_item(T, 0, pivot_index);
    double pivot = tea_get_number(T, 1);
    tea_pop(T, 1);

    int i = start - 1;
    int j = end + 1;

    double item;
    while(true)
    {
        do
        {
            i = i + 1;
            tea_get_item(T, 0, i);
            item = tea_get_number(T, 1);
            tea_pop(T, 1);
        } 
        while(item < pivot);

        do 
        {
            j = j - 1;
            tea_get_item(T, 0, j);
            item = tea_get_number(T, 1);
            tea_pop(T, 1);
        } 
        while(item > pivot);

        if(i >= j) 
        {
            return j;
        }

        // Swap arr[i] with arr[j]
        tea_get_item(T, 0, i);
        tea_get_item(T, 0, j);
        tea_set_item(T, 0, i);
        tea_set_item(T, 0, j);
    }
}

// Implementation of Quick Sort using the Hoare
// Partition scheme
// Best Case O(n log n)
// Worst Case O(n^2) (If the list is already sorted) 
static void quicksort(TeaState* T, int start, int end) 
{
    while(start < end) 
    {
        int part = partition(T, start, end);

        // Recurse for the smaller halve
        if(part - start < end - part) 
        {
            quicksort(T, start, part);
            start = start + 1;
        } 
        else 
        {
            quicksort(T, part + 1, end);
            end = end - 1;
        }
    }
}

static void list_sort(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    int len = tea_len(T, 0);

    // Check if all the list elements are indeed numbers.
    for(int i = 0; i < len; i++) 
    {
        tea_get_item(T, 0, i);
        if(!tea_is_number(T, 1))
        {
            tea_error(T, "sort() takes lists with numbers (index %d was not a number)", i);
        }
        tea_pop(T, 1);
    }

    quicksort(T, 0, len - 1);
}

static void list_index(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    int len = tea_len(T, 0);

    for(int i = 0; i < len; i++) 
    {
        tea_get_item(T, 0, i);
        if(tea_equals(T, 1, 2)) 
        {
            tea_push_number(T, i);
            return;
        }
        tea_pop(T, 1);
    }

    tea_push_null(T);
}

static void list_join(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 0 or 1 argument, got %d", count);

    int len = tea_len(T, 0);
    if(len == 0)
    {
        tea_push_lstring(T, "", 0);
        return;
    }

    const char* sep = "";
    int sep_len = 0;
    if(count == 2)
    {
        sep = tea_check_lstring(T, 1, &sep_len);
    }

    char* output;
    char* string = NULL;
    int length = 0;
    int element_len;

    for(int i = 0; i < len - 1; i++)
    {
        tea_get_item(T, 0, i);
        output = (char*)tea_to_lstring(T, count, &element_len);

        string = GROW_ARRAY(T, char, string, length, length + element_len + sep_len);

        memcpy(string + length, output, element_len);
        length += element_len;
        memcpy(string + length, sep, sep_len);
        length += sep_len;
        tea_pop(T, 2);
    }

    tea_get_item(T, 0, len - 1);
    // Outside the loop as we do not want the append the delimiter on the last element
    output = (char*)tea_to_lstring(T, count, &element_len);

    string = GROW_ARRAY(T, char, string, length, length + element_len + 1);
    memcpy(string + length, output, element_len);
    length += element_len;
    string[length] = '\0';
    tea_pop(T, 2);

    tea_push_slot(T, OBJECT_VAL(tea_take_string(T, string, length)));
}

static void list_copy(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 1);

    int len = tea_len(T, 0);

    tea_push_list(T);

    for(int i = 0; i < len; i++) 
    {
        tea_get_item(T, 0, i);
        tea_add_item(T, 1);
    }
}

static void list_iterate(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    int len = tea_len(T, 0);

    // If we're starting the iteration, return the first index.
    if(tea_is_null(T, 1))
    {
        if(len == 0)
        {
            tea_push_null(T);
            return;
        }
        tea_push_number(T, 0);
        return;
    }

    if(!tea_is_number(T, 1))
    {
        tea_error(T, "Expected a number to iterate");
        return;
    }

    int index = tea_get_number(T, 1);
    // Stop if we're out of bounds
    if(index < 0 || index >= len - 1)
    {
        tea_push_null(T);
        return;
    }

    // Otherwise, move to the next index
    tea_push_number(T, index + 1);
}

static void list_iteratorvalue(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 2);

    int index = tea_check_number(T, 1);
    tea_get_item(T, 0, index);
}

static const TeaClass list_class[] = {
    { "len", "property", list_len },
    { "add", "method", list_add },
    { "remove", "method", list_remove },
    { "delete", "method", list_delete },
    { "clear", "method", list_clear },
    { "insert", "method", list_insert },
    { "extend", "method", list_extend },
    { "reverse", "method", list_reverse },
    { "contains", "method", list_contains },
    { "count", "method", list_count },
    { "swap", "method", list_swap },
    { "fill", "method", list_fill },
    { "sort", "method", list_sort },
    { "index", "method", list_index },
    { "join", "method", list_join },
    { "copy", "method", list_copy },
    { "iterate", "method", list_iterate },
    { "iteratorvalue", "method", list_iteratorvalue },
    { NULL, NULL, NULL }
};

void tea_open_list(TeaState* T)
{
    tea_create_class(T, TEA_LIST_CLASS, list_class);
    T->list_class = AS_CLASS(T->slot[T->top - 1]);
    tea_set_global(T, TEA_LIST_CLASS);
}