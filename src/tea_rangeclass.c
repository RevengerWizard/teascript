// tea_file.c
// Teascript range class

#include <math.h>

#include "tea.h"

#include "tea_vm.h"
#include "tea_core.h"

static void range_start(TeaState* T)
{
    double start;
    tea_get_range(T, 0, &start, NULL, NULL);
    tea_push_number(T, start);
}

static void range_end(TeaState* T)
{
    double end;
    tea_get_range(T, 0, NULL, &end, NULL);
    tea_push_number(T, end);
}

static void range_step(TeaState* T)
{
    double step;
    tea_get_range(T, 0, NULL, NULL, &step);
    tea_push_number(T, step);
}

static void range_len(TeaState* T)
{
    double start, end, step;
    tea_get_range(T, 0, &start, &end, &step);
    tea_push_number(T, (end - start) / step);
}

static void range_iterate(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count != 2, "Expected 2 arguments, got %d", count);
    
    double start, end;
    tea_check_range(T, 0, &start, &end, NULL);

    // Empty range
    if(start == end)
    {
        tea_push_null(T);
        return;
    }

    // Start the iteration
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

    // Iterate towards [end] from [start]
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

static void range_iteratorvalue(TeaState* T)
{
    int count = tea_get_top(T);
    tea_check_args(T, count != 2, "Expected 2 arguments, got %d", count);
}

static void range_constructor(TeaState* T)
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

static void range_contains(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 2);

    double number = tea_check_number(T, 1);
    double start, end, step;
    tea_get_range(T, 0, &start, &end, &step);

    tea_push_bool(T, !(number < start || number > end) && (fmod(number, step) == 0));
}

static void range_reverse(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_max_args(T, count, 1);

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

static const TeaClass range_class[] = {
    { "start", "property", range_start },
    { "end", "property", range_end },
    { "step", "property", range_step },
    { "len", "property", range_len },
    { "constructor", "method", range_constructor },
    { "contains", "method", range_contains },
    { "reverse", "method", range_reverse },
    { "iterate", "method", range_iterate },
    { "iteratorvalue", "method", range_iteratorvalue },
    { NULL, NULL, NULL }
};

void tea_open_range(TeaState* T)
{
    tea_create_class(T, TEA_RANGE_CLASS, range_class);
    T->range_class = AS_CLASS(T->top[-1]);
    tea_set_global(T, TEA_RANGE_CLASS);
    tea_push_null(T);
}