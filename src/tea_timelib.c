/*
** tea_timelib.c
** Teascript time module
*/

#include <stdlib.h>
#include <time.h>

#define tea_timelib_c
#define TEA_LIB

#include "tea.h"

#include "tea_import.h"
#include "tea_core.h"

static void time_clock(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 0);
    tea_push_number(T, ((double)clock()) / ((double)CLOCKS_PER_SEC));
}

static void time_time(TeaState* T)
{
    int count = tea_get_top(T);
    tea_ensure_min_args(T, count, 0);
    tea_push_number(T, (double)time(NULL));
}

static const TeaModule time_module[] = {
    { "clock", time_clock },
    { "time", time_time },
    { NULL, NULL }
};

void tea_import_time(TeaState* T)
{
    tea_create_module(T, TEA_TIME_MODULE, time_module);
}