/*
** lib_list.c
** Teascript List class
*/

#define lib_list_c
#define TEA_CORE

#include <math.h>

#include "tea.h"
#include "tealib.h"

#include "tea_vm.h"
#include "tea_gc.h"
#include "tea_str.h"

static void list_len(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    tea_push_number(T, tea_len(T, 0));
}

static void list_constructor(tea_State* T)
{
    tea_new_list(T);
}

static void list_add(tea_State* T)
{
    tea_add_item(T, 0);
    tea_set_top(T, 1);
}

static void list_remove(tea_State* T)
{
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
            if(!found && tea_equal(T, 1, 2))
            {
                found = true;
            }
            tea_pop(T, 1);

            /* If we have found the value, shuffle the array */
            if(found)
            {
                tea_get_item(T, 0, i + 1);
                tea_set_item(T, 0, i);
            }
        }

        /* Check if it's the last element */
        tea_get_item(T, 0, len - 1);
        if(!found && tea_equal(T, 1, 2))
        {
            found = true;
        }
        tea_pop(T, 1);
    }
    else
    {
        tea_get_item(T, 0, 0);
        if(tea_equal(T, 1, 2))
        {
            found = true;
        }
        tea_pop(T, 1);
    }

    if(found)
    {
        AS_LIST(T->base[0])->count--;
        tea_pop(T, 1);
        return;
    }

    tea_error(T, "TValue does not exist within the list");
}

static void list_delete(tea_State* T)
{
    int len = tea_len(T, 0);

    if(len == 0)
    {
        tea_pop(T, 1);
        return;
    }

    int index = tea_check_number(T, 1);

    if(index < 0 || index > len)
    {
        tea_error(T, "Index out of bounds");
    }

    for(int i = index; i < len - 1; i++)
    {
        tea_get_item(T, 0, i + 1);
        tea_set_item(T, 0, i);
    }

    AS_LIST(T->base[0])->count--;
    tea_pop(T, 1);
}

static void list_clear(tea_State* T)
{
    GClist* list = AS_LIST(T->base[0]);
    list->items = NULL;
    list->count = 0;
    list->size = 0;
}

static void list_insert(tea_State* T)
{
    GClist* list = AS_LIST(T->base[0]);
    TValue insert_value = T->base[1];
    int index = tea_check_number(T, 2);

    if(index < 0 || index > list->count)
    {
        tea_error(T, "Index out of bounds for the list given");
    }

    if(list->size < list->count + 1)
    {
        int old_size = list->size;
        list->size = TEA_MEM_GROW(old_size);
        list->items = tea_mem_reallocvec(T, TValue, list->items, old_size, list->size);
    }

    list->count++;

    for(int i = list->count - 1; i > index; --i)
    {
        list->items[i] = list->items[i - 1];
    }

    list->items[index] = insert_value;
    tea_pop(T, 2);
}

static void list_extend(tea_State* T)
{
    tea_check_list(T, 1);
    int len = tea_len(T, 1);

    /* list index 0, list index 1 */
    for(int i = 0; i < len; i++)
    {
        tea_get_item(T, 1, i);
        tea_add_item(T, 0);
    }
    tea_pop(T, 1);
}

static void list_reverse(tea_State* T)
{
    int len = tea_len(T, 0);
    for(int i = 0; i < len / 2; i++)
    {
        tea_get_item(T, 0, i);
        tea_get_item(T, 0, len - i - 1);
        tea_set_item(T, 0, i);
        tea_set_item(T, 0, len - i - 1);
    }
}

static void list_contains(tea_State* T)
{
    int len = tea_len(T, 0);

    for(int i = 0; i < len; i++)
    {
        tea_get_item(T, 0, i);
        if(tea_equal(T, 1, 2))
        {
            tea_push_bool(T, true);
            return;
        }
        tea_pop(T, 1);
    }

    tea_push_bool(T, false);
}

static void list_count(tea_State* T)
{
    int len = tea_len(T, 0);

    int n = 0;
    for(int i = 0; i < len; i++)
    {
        tea_get_item(T, 0, i);
        if(tea_equal(T, 1, 2))
        {
            n++;
        }
        tea_pop(T, 1);
    }

    tea_push_number(T, n);
}

static void list_fill(tea_State* T)
{
    int len = tea_len(T, 0);

    for(int i = 0; i < len; i++)
    {
        tea_push_value(T, 1);
        tea_set_item(T, 0, i);
    }
    tea_pop(T, 1);
}

static void set2(tea_State* T, int i, int j)
{
    tea_set_item(T, 0, i);
    tea_set_item(T, 0, j);
}

static bool sort_comp(tea_State* T, int a, int b)
{
    if(!tea_is_null(T, 1))
    {
        bool res;
        tea_push_value(T, 1);
        tea_push_value(T, a - 1); /* -1 to compensate function */
        tea_push_value(T, b - 2); /* -2 to compensate function and 'a' */
        tea_call(T, 2);
        res = tea_check_bool(T, -1);
        tea_pop(T, 1);
        return res;
    }
    else
    {
        double n1 = tea_check_number(T, a);
        double n2 = tea_check_number(T, b);
        return n1 < n2;
    }
}

static void auxsort(tea_State* T, int l, int u)
{
    while(l < u)
    {
        int i, j;
        /* sort elements a[l], a[(l+u)/2] and a[u] */
        tea_get_item(T, 0, l);
        tea_get_item(T, 0, u);
        if(sort_comp(T, -1, -2)) /* a[u] < a[l]? */
            set2(T, l, u);        /* swap a[l] - a[u] */
        else
            tea_pop(T, 2);
        if(u - l == 1)
            break; /* only 2 elements */
        i = (l + u) / 2;
        tea_get_item(T, 0, i);
        tea_get_item(T, 0, l);
        if(sort_comp(T, -2, -1)) /* a[i]<a[l]? */
            set2(T, i, l);
        else
        {
            tea_pop(T, 1); /* remove a[l] */
            tea_get_item(T, 0, u);
            if(sort_comp(T, -1, -2)) /* a[u]<a[i]? */
                set2(T, i, u);
            else
                tea_pop(T, 2);
        }
        if(u - l == 2)
            break;            /* only 3 elements */
        tea_get_item(T, 0, i); /* Pivot */
        tea_push_value(T, -1);
        tea_get_item(T, 0, u - 1);
        set2(T, i, u - 1);
        /* a[l] <= P == a[u-1] <= a[u], only need to sort from l+1 to u-2 */
        i = l;
        j = u - 1;
        while(true)
        { /* invariant: a[l..i] <= P <= a[j..u] */
            /* repeat ++i until a[i] >= P */
            while(tea_get_item(T, 0, ++i), sort_comp(T, -1, -2))
            {
                if(i > u)
                    tea_error(T, "invalid order function for sorting");
                tea_pop(T, 1); /* remove a[i] */
            }
            /* repeat --j until a[j] <= P */
            while(tea_get_item(T, 0, --j), sort_comp(T, -3, -1))
            {
                if(j < l)
                    tea_error(T, "invalid order function for sorting");
                tea_pop(T, 1); /* remove a[j] */
            }
            if(j < i)
            {
                tea_pop(T, 3); /* pop pivot, a[i], a[j] */
                break;
            }
            set2(T, i, j);
        }
        tea_get_item(T, 0, u - 1);
        tea_get_item(T, 0, i);
        set2(T, u - 1, i); /* swap pivot (a[u-1]) with a[i] */
        /* a[l..i-1] <= a[i] == P <= a[i+1..u] */
        /* adjust so that smaller half is in [j..i] and larger one in [l..u] */
        if(i - l < u - i)
        {
            j = l;
            i = i - 1;
            l = i + 2;
        }
        else
        {
            j = i + 1;
            i = u;
            u = j - 2;
        }
        auxsort(T, j, i); /* call recursively the smaller one */
    }
}

static void list_sort(tea_State* T)
{
    int len = tea_len(T, 0);

    if(!tea_is_nonenull(T, 1))
        tea_check_function(T, 1);
    tea_set_top(T, 2);
    auxsort(T, 0, len - 1);
    tea_pop(T, 1);
}

static void list_index(tea_State* T)
{
    int len = tea_len(T, 0);

    for(int i = 0; i < len; i++)
    {
        tea_get_item(T, 0, i);
        if(tea_equal(T, 1, 2))
        {
            tea_push_number(T, i);
            return;
        }
        tea_pop(T, 1);
    }

    tea_push_null(T);
}

static void list_join(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 1 || count > 2, "Expected 0 or 1 argument, got %d", count);

    int list_len = tea_len(T, 0);
    if(list_len == 0)
    {
        tea_push_literal(T, "");
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
    int len = 0;
    int element_len;

    for(int i = 0; i < list_len - 1; i++)
    {
        tea_get_item(T, 0, i);
        output = (char*)tea_to_lstring(T, count, &element_len);

        string = tea_mem_reallocvec(T, char, string, len, len + element_len + sep_len);

        memcpy(string + len, output, element_len);
        len += element_len;
        memcpy(string + len, sep, sep_len);
        len += sep_len;
        tea_pop(T, 2);
    }

    tea_get_item(T, 0, list_len - 1);
    /* Outside the loop as we do not want the append the delimiter on the last element */
    output = (char*)tea_to_lstring(T, count, &element_len);

    string = tea_mem_reallocvec(T, char, string, len, len + element_len + 1);
    memcpy(string + len, output, element_len);
    len += element_len;
    string[len] = '\0';
    tea_pop(T, 2);

    tea_vm_push(T, OBJECT_VAL(tea_str_take(T, string, len)));
}

static void list_copy(tea_State* T)
{
    int len = tea_len(T, 0);

    tea_new_list(T);

    for(int i = 0; i < len; i++)
    {
        tea_get_item(T, 0, i);
        tea_add_item(T, 1);
    }
}

static void list_find(tea_State* T)
{
    tea_check_list(T, 0);
    tea_check_function(T, 1);

    int len = tea_len(T, 0);

    for(int i = 0; i < len; i++)
    {
        tea_push_value(T, 1);
        tea_get_item(T, 0, i);
        tea_call(T, 1);

        bool found = tea_check_bool(T, -1);
        tea_pop(T, 1);

        if(found)
        {
            tea_get_item(T, 0, i);
            return;
        }
    }
    tea_push_null(T);
}

static void flatten(tea_State* T, int src, int len)
{
    for(int i = 0; i < len; i++)
    {
        tea_get_item(T, src, i);
        if(tea_is_list(T, -1))
        {
            int l = tea_len(T, -1);
            int top = tea_get_top(T);
            flatten(T, top - 1, l);
        }
        else
        {
            tea_push_value(T, -1);
            tea_add_item(T, 1);
        }
        tea_pop(T, 1);
    }
}

static void list_flat(tea_State* T)
{
    int len = tea_len(T, 0);

    tea_new_list(T);
    flatten(T, 0, len);
}

static void list_map(tea_State* T)
{
    tea_check_list(T, 0);
    tea_check_function(T, 1);

    int len = tea_len(T, 0);

    tea_new_list(T);

    for(int i = 0; i < len; i++)
    {
        tea_push_value(T, 1);
        tea_get_item(T, 0, i);
        tea_call(T, 1);
        tea_add_item(T, 2);
    }
    tea_push_value(T, 2);
}

static void list_filter(tea_State* T)
{
    int len = tea_len(T, 0);

    tea_new_list(T);

    int j = 0;
    for(int i = 0; i < len; i++)
    {
        tea_push_value(T, 1);
        tea_get_item(T, 0, i);
        tea_call(T, 1);

        bool filter = tea_check_bool(T, -1);
        tea_pop(T, 1);

        if(filter)
        {
            tea_get_item(T, 0, i);
            tea_add_item(T, 2);
            j++;
        }
    }
    tea_push_value(T, 2);
}

static void list_reduce(tea_State* T)
{
    tea_check_list(T, 0);
    tea_check_function(T, 1);

    int len = tea_len(T, 0);
    if(len == 0)
    {
        tea_pop(T, 1);
        return;
    }

    int i = 0;
    tea_get_item(T, 0, i++);    /* pivot item */
    for(; i < len; i++)
    {
        tea_push_value(T, 1);
        tea_push_value(T, -2);      /* push pivot */
        tea_get_item(T, 0, i);
        tea_call(T, 2);
        tea_replace(T, -2);     /* replace pivot with newer item */
    }
}

static void list_foreach(tea_State* T)
{
    tea_check_list(T, 0);
    tea_check_function(T, 1);

    int len = tea_len(T, 0);

    for(int i = 0; i < len; i++)
    {
        tea_push_value(T, 1);
        tea_get_item(T, 0, i);
        tea_call(T, 1);
        tea_pop(T, 1);
    }
    tea_set_top(T, 1);
}

static void list_iterate(tea_State* T)
{
    int len = tea_len(T, 0);

    /* If we're starting the iteration, return the first index */
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
    /* Stop if we're out of bounds */
    if(index < 0 || index >= len - 1)
    {
        tea_push_null(T);
        return;
    }

    /* Otherwise, move to the next index */
    tea_push_number(T, index + 1);
}

static void list_iteratorvalue(tea_State* T)
{
    int index = tea_check_number(T, 1);
    tea_get_item(T, 0, index);
}

static void list_opadd(tea_State* T)
{
    tea_check_list(T, 0);
    tea_check_list(T, 1);
    
    GClist* l1 = AS_LIST(T->base[0]);
    GClist* l2 = AS_LIST(T->base[1]);

    GClist* list = tea_obj_new_list(T);
    tea_vm_push(T, OBJECT_VAL(list));

    for(int i = 0; i < l1->count; i++)
    {
        tea_list_append(T, list, l1->items[i]);
    }

    for(int i = 0; i < l2->count; i++)
    {
        tea_list_append(T, list, l2->items[i]);
    }

    tea_pop(T, 3);
    tea_vm_push(T, OBJECT_VAL(list));
}

static const tea_Class list_class[] = {
    { "len", "property", list_len, TEA_VARARGS },
    { "constructor", "method", list_constructor, 1 },
    { "add", "method", list_add, 2 },
    { "remove", "method", list_remove, 2 },
    { "delete", "method", list_delete, 2 },
    { "clear", "method", list_clear, 1 },
    { "insert", "method", list_insert, 3 },
    { "extend", "method", list_extend, 2 },
    { "reverse", "method", list_reverse, 1 },
    { "contains", "method", list_contains, 2 },
    { "count", "method", list_count, 2 },
    { "fill", "method", list_fill, 2 },
    { "sort", "method", list_sort, TEA_VARARGS },
    { "index", "method", list_index, 2 },
    { "join", "method", list_join, TEA_VARARGS },
    { "copy", "method", list_copy, 1 },
    { "find", "method", list_find, 2 },
    { "flat", "method", list_flat, 1 },
    { "map", "method", list_map, 2 },
    { "filter", "method", list_filter, 2 },
    { "reduce", "method", list_reduce, 2 },
    { "foreach", "method", list_foreach, 2 },
    { "iterate", "method", list_iterate, 2 },
    { "iteratorvalue", "method", list_iteratorvalue, 2 },
    { "+", "static", list_opadd, 2 },
    { NULL, NULL, NULL }
};

void tea_open_list(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_LIST, list_class);
    T->list_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_CLASS_LIST);
    tea_push_null(T);
}