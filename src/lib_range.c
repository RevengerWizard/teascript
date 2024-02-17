/*
** lib_range.c
** Teascript Range class
*/

#define lib_range_c
#define TEA_CORE

#include <math.h>

#include "tea.h"
#include "tealib.h"

#include "tea_vm.h"

static void range_start(tea_State* T)
{
    int count = tea_get_top(T);
    GCrange* range = AS_RANGE(T->base[0]);
    double start;
    if(count == 1)
    {
        start = range->start;
    }
    else
    {
        start = tea_check_number(T, 1);
        range->start = start;
    }
    tea_push_number(T, start);
}

static void range_end(tea_State* T)
{
    int count = tea_get_top(T);
    GCrange* range = AS_RANGE(T->base[0]);
    double end;
    if(count == 1)
    {
        end = range->end;
    }
    else
    {
        end = tea_check_number(T, 1);
        range->end = end;
    }
    tea_push_number(T, end);
}

static void range_step(tea_State* T)
{
    int count = tea_get_top(T);
    GCrange* range = AS_RANGE(T->base[0]);
    double step;
    if(count == 1)
    {
        step = range->step;
    }
    else
    {
        step = tea_check_number(T, 1);
        range->step = step;
    }
    tea_push_number(T, step);
}

static void range_len(tea_State* T)
{
    if(tea_get_top(T) != 1) tea_error(T, "readonly property");
    double start, end, step;
    tea_get_range(T, 0, &start, &end, &step);
    tea_push_number(T, (end - start) / step);
}

static void range_iterate(tea_State* T)
{
    double start, end;
    tea_check_range(T, 0, &start, &end, NULL);

    /* Empty range */
    if(start == end)
    {
        tea_push_null(T);
        return;
    }

    /* Start the iteration */
    if(tea_is_null(T, 1))
    {
        tea_push_number(T, start);
        return;
    }

    if(!tea_is_number(T, 1))
    {
        tea_error(T, "Expected a number to iterate");
    }

    int iterator = tea_get_number(T, 1);

    /* Iterate towards [end] from [start] */
    if(start < end)
    {
        iterator++;
        if(iterator > end)
        {
            tea_push_null(T);
            return;
        }
    }
    else
    {
        iterator--;
        if(iterator < end)
        {
            tea_push_null(T);
            return;
        }
    }
    if(iterator == end)
    {
        tea_push_null(T);
        return;
    }

    tea_push_number(T, iterator);
}

static void range_iteratorvalue(tea_State* T) {}

static void range_constructor(tea_State* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count < 2 || count > 4, "Expected 1 argument up to 3, got %d", count);
    if(count == 2)
    {
        tea_push_range(T, 0, tea_check_number(T, 1), 1);
    }
    else if(count == 3)
    {
        tea_push_range(T, tea_check_number(T, 1), tea_check_number(T, 2), 1);
    }
    else
    {
        tea_push_range(T, tea_check_number(T, 1), tea_check_number(T, 2), tea_check_number(T, 3));
    }
}

static void range_contains(tea_State* T)
{
    double number = tea_check_number(T, 1);
    double start, end, step;
    tea_get_range(T, 0, &start, &end, &step);

    tea_push_bool(T, !(number < start || number > end) && (fmod(number, step) == 0));
}

static void range_reverse(tea_State* T)
{
    double start, end, step;
    tea_get_range(T, 0, &start, &end, &step);

    double new_start, new_end;
    if (step > 0)
    {
        new_start = end;
        new_end = start;
    }
    else
    {
        new_start = start;
        new_end = end;
    }

    double new_step = -step;
    tea_push_range(T, new_start, new_end, new_step);
}

static const tea_Class range_class[] = {
    { "start", "property", range_start, TEA_VARARGS },
    { "end", "property", range_end, TEA_VARARGS },
    { "step", "property", range_step, TEA_VARARGS },
    { "len", "property", range_len, TEA_VARARGS },
    { "constructor", "method", range_constructor, TEA_VARARGS },
    { "contains", "method", range_contains, 2 },
    { "reverse", "method", range_reverse, 1 },
    { "iterate", "method", range_iterate, 2 },
    { "iteratorvalue", "method", range_iteratorvalue, 2 },
    { NULL, NULL, NULL }
};

void tea_open_range(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_RANGE, range_class);
    T->range_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_CLASS_RANGE);
    tea_push_null(T);
}