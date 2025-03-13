/*
** lib_range.c
** Teascript Range class
*/

#include <math.h>

#define lib_range_c
#define TEA_CORE

#include "tea.h"
#include "tealib.h"

#include "tea_lib.h"

static void range_start(tea_State* T)
{
    tea_push_number(T, rangeV(T->base)->start);
}

static void range_end(tea_State* T)
{
    tea_push_number(T, rangeV(T->base)->end);
}

static void range_step(tea_State* T)
{
    tea_push_number(T, rangeV(T->base)->step);
}

static void range_len(tea_State* T)
{
    double start, end, step;
    tea_get_range(T, 0, &start, &end, &step);
    tea_push_number(T, (end - start) / step);
}

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

static void range_iternext(tea_State* T)
{
    double curr = tea_get_number(T, tea_upvalue_index(0));
    double end = tea_get_number(T, tea_upvalue_index(1));
    double step = tea_get_number(T, tea_upvalue_index(2));

    /* Check if iteration is complete */
    if((step > 0 && curr >= end) || (step < 0 && curr <= end) || step == 0)
    {
        //puts("push nill please");
        tea_push_nil(T);
        return;
    }
    
    /* Push current value */
    tea_push_number(T, curr);
    curr += step;
    
    /* Update current for next iteration */
    tea_push_number(T, curr);
    tea_replace(T, tea_upvalue_index(0));
}

static void range_iter(tea_State* T)
{
    GCrange* range = tea_lib_checkrange(T, 0);
    tea_push_number(T, range->start);
    tea_push_number(T, range->end);
    tea_push_number(T, range->step);
    tea_push_cclosure(T, range_iternext, 3, 0, 0);
}

/* ------------------------------------------------------------------------ */

static const tea_Methods range_reg[] = {
    { "start", "getter", range_start, 1, 0 },
    { "end", "getter", range_end, 1, 0 },
    { "step", "getter", range_step, 1, 0 },
    { "len", "getter", range_len, 1, 0 },
    { "new", "method", range_init, 2, 2 },
    { "contains", "method", range_contains, 2, 0 },
    { "reverse", "method", range_reverse, 1, 0 },
    { "copy", "method", range_copy, 1, 0 },
    { "iter", "method", range_iter, 1, 0 },
    { NULL, NULL, NULL }
};

void tea_open_range(tea_State* T)
{
    tea_create_class(T, TEA_CLASS_RANGE, range_reg);
    T->gcroot[GCROOT_KLRANGE] = obj2gco(classV(T->top - 1));
    tea_set_global(T, TEA_CLASS_RANGE);
    tea_push_nil(T);
}