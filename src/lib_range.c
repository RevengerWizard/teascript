/*
** lib_range.c
** Teascript Range class
*/

#include <math.h>

#define lib_range_c
#define TEA_CORE

#include "tea.h"
#include "tealib.h"

#include "tea_vm.h"
#include "tea_lib.h"

static void range_start(tea_State* T)
{
    int count = tea_get_top(T);
    GCrange* range = rangeV(T->base);
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
    GCrange* range = rangeV(T->base);
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
    GCrange* range = rangeV(T->base);
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
        tea_push_nil(T);
        return;
    }

    /* Start the iteration */
    if(tea_is_nil(T, 1))
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
            tea_push_nil(T);
            return;
        }
    }
    else
    {
        iterator--;
        if(iterator < end)
        {
            tea_push_nil(T);
            return;
        }
    }
    if(iterator == end)
    {
        tea_push_nil(T);
        return;
    }

    tea_push_number(T, iterator);
}

static void range_iteratorvalue(tea_State* T) {}

static void range_init(tea_State* T)
{
    int count = tea_get_top(T);
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
    GCrange* range = tea_lib_checkrange(T, 0);
    double start, end, step;
    if(range->step > 0)
    {
        start = range->end;
        end = range->start;
    }
    else
    {
        start = range->start;
        end = range->end;
    }
    step = -range->step;
    /* Reverse the range */
    range->start = start;
    range->end = end;
    range->step = step;
}

static void range_copy(tea_State* T)
{
    GCrange* range = tea_lib_checkrange(T, 0);
    GCrange* newrange = tea_range_new(T, range->start, range->end, range->step);
    setrangeV(T, T->top++, newrange);
}

/* ------------------------------------------------------------------------ */

static const tea_Methods range_class[] = {
    { "start", "property", range_start, TEA_VARARGS },
    { "end", "property", range_end, TEA_VARARGS },
    { "step", "property", range_step, TEA_VARARGS },
    { "len", "property", range_len, TEA_VARARGS },
    { "new", "method", range_init, -4 },
    { "contains", "method", range_contains, 2 },
    { "reverse", "method", range_reverse, 1 },
    { "copy", "method", range_copy, 1 },
    { "iterate", "method", range_iterate, 2 },
    { "iteratorvalue", "method", range_iteratorvalue, 2 },
    { NULL, NULL, NULL }
};

void tea_open_range(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_RANGE, range_class);
    T->range_class = classV(T->top - 1);
    tea_set_global(T, TEA_CLASS_RANGE);
    tea_push_nil(T);
}